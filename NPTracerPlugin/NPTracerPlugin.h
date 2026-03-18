#pragma once

#include <CH/CH_LocalVariable.h>
#include <OP/OP_Context.h>
#include <OP/OP_Error.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <OP/OP_Operator.h>
#include <PRM/PRM_Template.h>
#include <SOP/SOP_Node.h>
#include <sys/SYS_Types.h>
#include <UT/UT_String.h>

#include <cstdint>
#include <limits>

#define CLAMP_UINT64_TO_UINT32(_val)                                                               \
    static_cast<uint32_t>(std::clamp(static_cast<uint64_t>(_val),                                  \
                                     static_cast<uint64_t>(std::numeric_limits<uint32_t>::min()),  \
                                     static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())))

namespace HDK_Sample
{
class SOP_Lsystem : public SOP_Node
{
public:
    static OP_Node* nodeConstructor(OP_Network*, const char*, OP_Operator*);

    /// Stores the description of the interface of the SOP in Houdini.
    /// Each parm template refers to a parameter.
    static PRM_Template templateList[];

    /// This optional data stores the list of local variables.
    static CH_LocalVariable localVariables[];

protected:
    SOP_Lsystem(OP_Network* net, const char* name, OP_Operator* op);
    virtual ~SOP_Lsystem();

    /// Disable parameters according to other parameters.
    virtual unsigned disableParms();

    /// cookMySop does the actual work of the SOP computing, in this
    /// case, a LSYSTEM
    virtual OP_ERROR cookMySop(OP_Context& context);
    OP_ERROR parseGrammar(OP_Context& context, std::string* outGrammar);

    /// This function is used to lookup local variables that you have
    /// defined specific to your SOP.
    virtual bool evalVariableValue(fpreal& val, int index, int thread);

    // Add virtual overload that delegates to the super class to avoid
    // shadow warnings.
    inline virtual bool evalVariableValue(UT_String& v, int i, int thread)
    {
        return SOP_Node::evalVariableValue(v, i, thread);
    }

private:
    // very explicit typing, oh well

    inline float ANGLE(fpreal t) const
    {
        fpreal a = evalFloat("angle", 0, t);
        return static_cast<float>(a);
    }

    inline float STEPSIZE(fpreal t) const
    {
        fpreal ss = evalFloat("stepSize", 0, t);
        return static_cast<float>(ss);
    }

    inline size_t ITERATIONS(fpreal t) const
    {
        exint it = evalInt("iterations", 0, t);
        return static_cast<size_t>(it);
    }

    inline float RADIUS(fpreal t) const
    {
        fpreal rd = evalFloat("radius", 0, t);
        return static_cast<float>(rd);
    }

    inline uint32_t DIVISIONS(fpreal t) const
    {
        exint dv = evalInt("divisions", 0, t);

        // user should never pass in a variable this big, but just in case
        return CLAMP_UINT64_TO_UINT32(dv);
    }

    inline UT_String GRAMMARFILE(fpreal t) const
    {
        UT_String s;
        evalString(s, "grammarFile", 0, t);
        return s;
    }

    inline UT_String GRAMMARSTRING(fpreal t) const
    {
        UT_String s;
        evalString(s, "grammarString", 0, t);
        return s;
    }

    uint32_t currPoint;
    uint32_t totalPoints;
};
}  // namespace HDK_Sample
