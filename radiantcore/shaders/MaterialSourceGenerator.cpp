#include "MaterialSourceGenerator.h"

#include "ShaderTemplate.h"
#include "string/replace.h"

namespace shaders
{

std::ostream& operator<<(std::ostream& stream, ShaderTemplate& shaderTemplate)
{
    stream << "\n";

    if (!shaderTemplate.getDescription().empty())
    {
        stream << "\tdescription \"" << string::replace_all_copy(shaderTemplate.getDescription(), "\"", "'") << "\"\n";
    }

    for (const auto& layer : shaderTemplate.getLayers())
    {
        stream << "\t{\n";

        stream << "\t}\n";
    }

    return stream;
}

std::string MaterialSourceGenerator::GenerateDefinitionBlock(ShaderTemplate& shaderTemplate)
{
    std::stringstream output;

    output << shaderTemplate;

    return output.str();
}

}
