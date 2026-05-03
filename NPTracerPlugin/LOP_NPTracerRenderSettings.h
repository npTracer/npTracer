#pragma once

#include <LOP/LOP_Node.h>
#include <PRM/PRM_Template.h>
#include <CH/CH_LocalVariable.h>

// TODO
// currently a minimal implementation of a LOP node
class LOP_NPTracerRenderSettings : public LOP_Node
{
public:
    static constexpr char INTERNAL_NAME[] = "lop_npTracerRenderSettings";
    static constexpr char UI_NAME[] = "NPTracer Render Settings LOP";
    static PRM_Template TEMPLATE_LIST[];
    static CH_LocalVariable LOCAL_VARIABLES[];

    static OP_Node* sNodeConstructor(OP_Network* parent, const char* name, OP_Operator* entry)
    {
        return new LOP_NPTracerRenderSettings(parent, name, entry);
    }

protected:
    LOP_NPTracerRenderSettings(OP_Network* parent, const char* name, OP_Operator* entry);

    virtual ~LOP_NPTracerRenderSettings() override = default;

    OP_ERROR cookMyLop(OP_Context& context) override;
};
