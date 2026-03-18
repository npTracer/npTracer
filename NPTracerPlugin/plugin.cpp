#include <DM/DM_RenderTable.h>
#include <HOM/HOM_Module.h>
#include <OP/OP_OperatorTable.h>
#include <UT/UT_DSOVersion.h>  // required header for Houdini to recognize this header as a plugin entrypoint

static void check_houdini_version()
{
    const char build_version[] = SYS_VERSION_MAJOR "." SYS_VERSION_MINOR "." SYS_VERSION_BUILD;
    std::string runtime_version;
    try
    {
        runtime_version = ::HOM().applicationVersionString();
    }
    catch (...)
    {
        /*
            According to header comments, this should not happen. But it does
            when running hython. If we ever want the warning for that case, we
            could possibly parse ${HFS}/toolkit/include/SYS/SYS_Version.h
        */
        return;
    }

    /*
        Check the version only once. We try multiple times because of the above
        failure case. With hython, the call from newVopOperator() works. But we
        still want to try checking as early as possible when not in hython.
    */
    static bool checked = false;
    if (checked)
    {
        return;
    }
    checked = true;

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

/**
 * @brief ROP node definition here
 * @param io_table
 * @return
 */
extern "C" SYS_VISIBILITY_EXPORT void newDriverOperator(OP_OperatorTable* io_table)
{
    check_houdini_version();
}

/**
 * @brief LOP node definition here
 * @param io_table
 * @return
 */
extern "C" SYS_VISIBILITY_EXPORT void newLopOperator(OP_OperatorTable* io_table)
{
    check_houdini_version();
}

/**
 * @brief VOP node definition here
 * @param io_table
 * @return
 */
extern "C" SYS_VISIBILITY_EXPORT void newVopOperator(OP_OperatorTable* io_table)
{
    check_houdini_version();
}

/**
 * @brief custom node type definition here
 * @param io_table
 * @return
 */
extern "C" SYS_VISIBILITY_EXPORT void newObjectOperator(OP_OperatorTable* io_table)
{
    check_houdini_version();
}

/**
 * @brief render hook here
 * @param io_table
 * @return
 */
extern "C" SYS_VISIBILITY_EXPORT void newRenderHook(DM_RenderTable* io_table)
{
    check_houdini_version();
}
