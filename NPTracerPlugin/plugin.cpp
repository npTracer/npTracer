#include "LOP_NPTracerRenderSettings.h"

#include <DM/DM_RenderTable.h>
#include <HOM/HOM_Module.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_DSOVersion.h>  // required header for Houdini to recognize this header as a plugin entrypoint

static void check_houdini_version()
{
    constexpr char build_version[] = SYS_VERSION_MAJOR "." SYS_VERSION_MINOR "." SYS_VERSION_BUILD;
    const std::string runtime_version = HOM().applicationVersionString();

    static bool sChecked = false;
    if (sChecked) return;

    sChecked = true;

    if (runtime_version != build_version)
    {
        std::cerr << "NPTracer for Houdini: This plugin is configured to be "
                     "used with Houdini "
                  << build_version << ".\n"
                  << "Using it with another version of Houdini (currently, " << runtime_version
                  << ") might result in crashes and other problems." << std::endl;
    }
}

extern "C" SYS_VISIBILITY_EXPORT void HoudiniDSOInit(UT_DSOInfo&)
{
    check_houdini_version();
}

extern "C" SYS_VISIBILITY_EXPORT void newLopOperator(OP_OperatorTable* table)
{
    check_houdini_version();

    table->addOperator(
        new OP_Operator(LOP_NPTracerRenderSettings::INTERNAL_NAME,  // Internal name
                        LOP_NPTracerRenderSettings::UI_NAME,  // UI name
                        LOP_NPTracerRenderSettings::sNodeConstructor,  // How to build the node
                        LOP_NPTracerRenderSettings::TEMPLATE_LIST,  // parameters
                        0,  // Min inputs
                        0,  // Max inputs
                        LOP_NPTracerRenderSettings::LOCAL_VARIABLES,  // Local variables
                        0)  // Operator flags
    );
}
