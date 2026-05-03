#include "LOP_NPTracerRenderSettings.h"

PRM_Template LOP_NPTracerRenderSettings::TEMPLATE_LIST[] = {
    PRM_Template()  // a sentinel template is needed to signal end of list
};

CH_LocalVariable LOP_NPTracerRenderSettings::LOCAL_VARIABLES[] = {
    { 0, 0, 0 },  // a sentinel variable is needed to signal end of list
};

LOP_NPTracerRenderSettings::LOP_NPTracerRenderSettings(OP_Network* parent, const char* name,
                                                       OP_Operator* entry)
    : LOP_Node(parent, name, entry)
{
}

OP_ERROR LOP_NPTracerRenderSettings::cookMyLop(OP_Context& context)
{
    return error(context);
}
