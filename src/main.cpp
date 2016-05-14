#include <iostream>
#include <fstream>
#include <algorithm>

#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "rapidxml_print.hpp"

namespace {

struct range {
    std::size_t begin;
    std::size_t end;

    range(std::size_t begin, std::size_t end) : begin(begin), end(end) {}
};

bool verbose = false;

//The documents

rapidxml::xml_document<> source_doc;
rapidxml::xml_document<> target_doc;

rapidxml::xml_node<>* copy_package(rapidxml::xml_document<>& source_doc, rapidxml::xml_node<>* package_node){
    //Create the "package" node (rates are not copied on purpose)
    auto package_target = target_doc.allocate_node(rapidxml::node_element, "package");
    package_target->append_attribute(target_doc.allocate_attribute("name", package_node->first_attribute("name")->value()));
    package_target->append_attribute(target_doc.allocate_attribute("complexity", package_node->first_attribute("complexity")->value()));

    //Create the "classes" node
    auto classes_target = target_doc.allocate_node(rapidxml::node_element, "classes");
    package_target->append_node(classes_target);

    auto classes_sources = package_node->first_node("classes");

    for(auto* class_source = classes_sources->first_node("class"); class_source; class_source = class_source->next_sibling()){
        std::string class_branch_rate(class_source->first_attribute("branch-rate")->value());
        std::string class_line_rate(class_source->first_attribute("line-rate")->value());

        //Create the "class" node
        auto class_target = target_doc.allocate_node(rapidxml::node_element, "classes");

        std::string filename(class_source->first_attribute("filename")->value());

        class_target->append_attribute(target_doc.allocate_attribute("name", class_source->first_attribute("name")->value()));
        class_target->append_attribute(target_doc.allocate_attribute("complexity", class_source->first_attribute("complexity")->value()));
        class_target->append_attribute(target_doc.allocate_attribute("filename", class_source->first_attribute("filename")->value()));

        // Copy directly the "methods" node
        auto* target_methods_node = source_doc.clone_node(class_source->first_node("methods"));
        class_target->append_node(target_methods_node);

        std::vector<range> ignored_ranges;

        // Collect the ranges to ignore from the source file

        {
            std::ifstream is(filename);
            std::string line;

            std::size_t i           = 0;
            bool in_range           = false;
            std::size_t range_start = 0;

            while (std::getline(is, line)) {
                if (line.find("COVERAGE_EXCLUDE_BEGIN") != std::string::npos) {
                    range_start = i;
                    in_range = true;
                } else if (line.find("COVERAGE_EXCLUDE_END") != std::string::npos) {
                    if (in_range) {
                        ignored_ranges.emplace_back(range_start, i);
                        std::cout << "range " << range_start << ":" << i << std::endl;
                    }

                    in_range = false;
                } else {
                    // Left trim
                    line.erase(line.begin(), std::find_if(line.begin(), line.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));

                    if (line.find("cpp_unreachable(") == 0) {
                        ignored_ranges.emplace_back(i, i);
                    }
                }

                ++i;
            }
        }

        auto lines_source = class_source->first_node("lines");
        auto lines_target = target_doc.allocate_node(rapidxml::node_element, "lines");
        class_target->append_node(lines_target);

        for (auto* line_source = lines_source->first_node("line"); line_source; line_source = line_source->next_sibling()) {
            std::string line_str(line_source->first_attribute("number")->value());
            std::size_t line = std::atoi(line_str.c_str());

            bool exclude = false;

            for (auto& range : ignored_ranges) {
                if(line >= range.begin && line <= range.end){
                    exclude = true;
                    break;
                }
            }

            if (!exclude) {
                auto* line_target = source_doc.clone_node(line_source);
                lines_target->append_node(line_target);
            } else if (verbose) {
                std::cout << "Exclude " << filename << ":" << line << std::endl;
            }
        }

        classes_target->append_node(class_target);
    }

    return package_target;
}

} //end of anonymous namespace

int main(int argc, char* argv[]){
    //Argument counter
    std::size_t i = 1;

    //Collect the arguments
    for(; i < std::size_t(argc); ++i){
        std::string arg(argv[i]);

        if(arg == "--verbose"){
            verbose = true;
        } else {
            break;
        }
    }

    if(argc - i + 1 < 2){
        std::cout << "coverage_merger: not enough arguments" << std::endl;
        return 1;
    }

    //Collect the paths
    std::string source_path(argv[i]);
    std::string target_path(argv[i + 1]);

    if(verbose){
        std::cout << "Source file: " << source_path << std::endl;
        std::cout << "Target file: " << target_path << std::endl;
    }

    //Parse all documents

    rapidxml::file<> source_file(source_path.c_str());
    source_doc.parse<0>(source_file.data());

    std::cout << "Document parsed" << std::endl;

    //Find the root nodes
    auto source_root = source_doc.first_node("coverage");

    //Find the "packages" nodes
    auto packages_source = source_root->first_node("packages");

    //Create root node in target (rates attributes are not copied on purpose)
    auto* target_root = target_doc.allocate_node(rapidxml::node_element, "coverage");
    target_root->append_attribute(target_doc.allocate_attribute("version", source_root->first_attribute("version")->value()));
    target_root->append_attribute(target_doc.allocate_attribute("timestamp", source_root->first_attribute("timestamp")->value()));
    target_doc.append_node(target_root);

    //Copy directly the "sources" node
    auto* target_sources_node = source_doc.clone_node(source_root->first_node("sources"));
    target_root->append_node(target_sources_node);

    //Create the "packages" node
    auto packages_target = target_doc.allocate_node(rapidxml::node_element, "packages");
    target_root->append_node(packages_target);

    //1. Copy all relevant packages from source -> target

    for(auto* package_node = packages_source->first_node("package"); package_node; package_node = package_node->next_sibling()){
        std::string package_name(package_node->first_attribute("name")->value());
        std::string package_branch_rate(package_node->first_attribute("branch-rate")->value());
        std::string package_line_rate(package_node->first_attribute("line-rate")->value());

        if(verbose){
            std::cout << "Copy package " << package_name << " from source to target" << std::endl;
        }

        //Copy the package into the target (ignoring uncovered classes)
        packages_target->append_node(copy_package(source_doc, package_node));
    }

    //Write the target doc
    std::ofstream target_stream(target_path);
    target_stream << target_doc;
    target_stream.close();

    std::cout << "Documents have been processed and target has been written" << std::endl;

    return 0;
}
