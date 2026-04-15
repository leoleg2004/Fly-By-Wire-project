#include <iostream>
#include <regex>
#include <string>

int main() {
    std::string src_line = "  sprintf(activity_parameters.name, \"GeometryBroadcastner\");";
    std::regex name_re(R"DELIM(sprintf\s*\(\s*([a-zA-Z0-9_\[\]]+)\.name\s*,\s*"([^"]+)"\s*\))DELIM");
    std::smatch m;
    if (std::regex_search(src_line, m, name_re)) {
        std::cout << "MATCHED! " << m[1].str() << " " << m[2].str() << std::endl;
    } else {
        std::cout << "FAILED" << std::endl;
    }
    return 0;
}
