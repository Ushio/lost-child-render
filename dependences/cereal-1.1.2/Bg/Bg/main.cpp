//
//  main.cpp
//  Bg
//
//  Created by Nakazi_w0w on 3/10/16.
//  Copyright © 2016 wow. All rights reserved.
//

#include <iostream>

#include <cereal/archives/json.hpp>
#include <cereal/archives/xml.hpp>

struct MyClass
{
    float x, y, z;
    
    template<class Archive>
    void serialize(Archive & archive, const unsigned int version)
    {
        archive( x, y, z ); // serialize things by passing them to the archive
    }
};

int main(int argc, const char * argv[]) {
    std::ostringstream ss;
    {
        cereal::JSONOutputArchive archive(ss);
        
        MyClass value;
        value.x = 50000.5f;
        value.y = 6;
        value.z = 9;
        
        archive(value);
    }

    
    std::cout << ss.str();
    return 0;
}
