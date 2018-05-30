#include "Toolset.h"
#include "Component.h"
#include "PendingCommand.h"
#include "File.h"
#include "Project.h"
#include "view/filter.h"

template <bool usePrivDepsFromOthers = false>
struct Tarjan {
  // https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
  struct Info {
    size_t index;
    size_t lowlink;
    bool onStack = true;
  };

  Component* originalComponent;
  size_t index = 0;
  std::vector<Component*> stack;
  std::unordered_map<Component*, Info> info;
  std::vector<std::vector<Component*>> nodes;
  void StrongConnect(Component* c) {
    info[c].index = info[c].lowlink = index++;
    stack.push_back(c);
    for (auto& c2 : c->pubDeps) {
      if (info[c2].index == 0) {
        StrongConnect(c2);
        info[c].lowlink = std::min(info[c].lowlink, info[c2].lowlink);
      } else if (info[c2].onStack) {
        info[c].lowlink = std::min(info[c].lowlink, info[c2].index);
      }
    }
    bool shouldUsePrivDeps = (c == originalComponent) || usePrivDepsFromOthers;
    if (shouldUsePrivDeps) {
      for (auto& c2 : c->privDeps) {
        if (info[c2].index == 0) {
          StrongConnect(c2);
          info[c].lowlink = std::min(info[c].lowlink, info[c2].lowlink);
        } else if (info[c2].onStack) {
          info[c].lowlink = std::min(info[c].lowlink, info[c2].index);
        }
      }
    }
    if (info[c].lowlink == info[c].index) {
      auto it = std::find(stack.begin(), stack.end(), c);
      nodes.push_back(std::vector<Component*>(it, stack.end()));
      for (auto& ent : nodes.back()) {
        info[ent].onStack = false;
      }
      stack.resize(it - stack.begin());
    }
  }
};

static std::string getLibNameFor(Component& component) {
  // TODO: change commponent to dotted string before making
  return "lib" + component.root.string() + ".a";
}

static std::string getExeNameFor(Component& component) {
  printf("%s\n", component.root.string().c_str());
  if (component.root.string() != ".") {
    return component.root.string();
  }
  return boost::filesystem::canonical(component.root).filename().string();
}

std::vector<std::vector<Component*>> GetTransitiveAllDeps(Component& c) {
  Tarjan<true> t;
  t.originalComponent = &c;
  t.StrongConnect(&c);
  return t.nodes;
}

std::vector<std::vector<Component*>> GetTransitivePubDeps(Component& c) {
  Tarjan<false> t;
  t.originalComponent = &c;
  t.StrongConnect(&c);
  return t.nodes;
}

std::vector<Component*> flatten(std::vector<std::vector<Component*>> in) {
  std::vector<Component*> v;
  for (auto& p : in) {
    v.insert(v.end(), p.begin(), p.end());
  }
  return v;
}

void UbuntuToolset::CreateCommandsFor(Project& project, Component& component) {
  boost::filesystem::path outputFolder = component.root;
  std::vector<File*> objects;

  std::vector<Component*> includeDeps = flatten(GetTransitivePubDeps(component));
  includeDeps.erase(std::find(includeDeps.begin(), includeDeps.end(), &component));
  std::string includes;
  for (auto& d : includeDeps) {
    includes += " -I" + d->root.string();
  }

  for (auto& f : filter(component.files, [&project](File*f){ return project.IsCompilationUnit(f->path.extension().string()); })) {
    boost::filesystem::path outputFile = std::string("obj") / outputFolder / (f->path.stem().string() + ".o");
    File* of = project.CreateFile(component, outputFile);
    PendingCommand* pc = new PendingCommand("g++ -c -o " + outputFile.string() + " " + f->path.string() + includes);
    objects.push_back(of);
    pc->AddOutput(of);
    std::vector<File*> deps;
    deps.push_back(f);
    size_t index = 0;
    while (index != deps.size()) {
      pc->AddInput(deps[index]);
      for (File* input : deps[index]->dependencies)
        if (std::find(deps.begin(), deps.end(), input) == deps.end()) deps.push_back(input);
      index++;
    }
    component.commands.push_back(pc);
  }
  if (!objects.empty()) {
    std::string command;
    boost::filesystem::path outputFile;
    PendingCommand* pc;
    if (component.type != "executable") {
      outputFile = "lib/" + getLibNameFor(component);
      command = "ar rcs " + outputFile.string();
      for (auto& file : objects) {
        command += " " + file->path.string();
      }
      pc = new PendingCommand(command);
    } else {
      outputFile = "bin/" + getExeNameFor(component);
      command = "g++ -o " + outputFile.string();

      for (auto& file : objects) {
        command += " " + file->path.string();
      }
      command += " -Llib";
      std::vector<std::vector<Component*>> linkDeps = GetTransitiveAllDeps(component);
      std::reverse(linkDeps.begin(), linkDeps.end());
      for (auto& d : linkDeps) {
        if (d.size() == 1 || (d.size() == 2 && (d[0] == &component || d[1] == &component))) {
          if (d[0] != &component) {
            command += " -l" + d[0]->root.string();
          } else if (d.size() == 2) {
            command += " -l" + d[1]->root.string();
          }
        } else {
          command += " --start-group";
          for (auto& c : d) {
            if (c != &component) {
              command += " -l" + c->root.string();
            }
          }
          command += " --end-group";
        }
      }
      pc = new PendingCommand(command);
      for (auto& d : linkDeps) {
        for (auto& c : d) {
          if (c != &component) {
            pc->AddInput(project.CreateFile(*c, "lib/" + getLibNameFor(*c)));
          }
        }
      }
    }
    File* libraryFile = project.CreateFile(component, outputFile);
    pc->AddOutput(libraryFile);
    for (auto& file : objects) {
      pc->AddInput(file);
    }
    component.commands.push_back(pc);
  }
}

