#include "Analysis.h"
#include "Component.h"
#include "Configuration.h"
#include <boost/filesystem.hpp>
#include "Input.h"
#include <iostream>

class Project {
public:
    Project(int argc, const char** argv)
    {
        projectRoot = boost::filesystem::current_path();
    }
private:
    void LoadProject() {
        LoadFileList(components, files, projectRoot);

        std::unordered_map<std::string, std::string> includeLookup;
        std::unordered_map<std::string, std::set<std::string>> collisions;
        CreateIncludeLookupTable(files, includeLookup, collisions);
        MapFilesToComponents(components, files);
        ForgetEmptyComponents(components);
        std::unordered_map<std::string, std::vector<std::string>> ambiguous;
        MapIncludesToDependencies(includeLookup, ambiguous, components, files);
        for (auto &i : ambiguous) {
            for (auto &c : collisions[i.first]) {
                files.find(c)->second.hasInclude = true; // There is at least one include that might end up here.
            }
        }
        PropagateExternalIncludes(files);
        ExtractPublicDependencies(components);
    }
    void UnloadProject() {
        components.clear();
        files.clear();
    }
    boost::filesystem::path projectRoot;
    std::unordered_map<std::string, Component *> components;
    std::unordered_map<std::string, File> files;
};

int main(int argc, const char **argv) {
    Project op(argc, argv);
    return 0;
}
