#include "level.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static inline std::string trim(const std::string& s){
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a==std::string::npos) return ""; return s.substr(a, b-a+1);
}

bool Level::loadFromIni(const std::string& path){
    colliders_.clear();
    instances_.clear();
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)){
        auto l = trim(line);
        if (l.empty() || l[0]=='#' || l[0]==';' || l[0]=='[') continue;
        auto parseSizeVec3 = [](const std::string& src)->glm::vec3{
            // accepts N or AxBxC
            int a=1,b=1,c=1; char x;
            std::stringstream ss(src);
            if (src.find('x')!=std::string::npos || src.find('X')!=std::string::npos){
                ss >> a >> x >> b; if (ss && (ss.peek()=='x' || ss.peek()=='X')) { ss >> x >> c; }
            } else {
                ss.clear(); ss.str(src); ss >> a; b=a; c=a;
            }
            return glm::vec3((float)a,(float)b,(float)c);
        };
        if (l.rfind("voxel", 0) == 0){
            // voxel x y z size=3 or size=3x2x5
            size_t off = 5;
            std::stringstream ss(l.substr(off));
            float x=0,y=0,z=0; ss >> x >> y >> z;
            glm::vec3 size(1);
            size_t ps = l.find("size=");
            if (ps!=std::string::npos){
                std::string val = trim(l.substr(ps+5));
                // cut at first space/comment
                size_t sp = val.find_first_of(" \t#;");
                if (sp!=std::string::npos) val = val.substr(0, sp);
                size = parseSizeVec3(val);
            }
            glm::vec3 he = size * 0.5f;
            glm::vec3 cpos(x,y,z);
            colliders_.push_back({cpos - he, cpos + he});
            instances_.push_back({cpos, size});
        }
    }
    return true;
}
