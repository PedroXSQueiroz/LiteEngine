#include <vector>

using namespace std;

struct FBXMesh;

class Asset3dData {
    
public:
    Asset3dData(std::vector<FBXMesh> objects)
        :m_objects(objects){};
    
    std::vector<FBXMesh> m_objects;
};