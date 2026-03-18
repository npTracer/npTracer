#include "LSystemPlugin.h"

#include <CH/CH_LocalVariable.h>
#include <FS/FS_Reader.h>
#include <GU/GU_PrimPoly.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Range.h>
#include <PRM/PRM_Shared.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_Template.h>
#include <UT/UT_DSOVersion.h>  // required header for Houdini to recognize this header as a plugin entrypoint
#include <UT/UT_Interrupt.h>

#include "LSystem.h"

using namespace HDK_Sample;

// houdini SOP registration entrypoint function
void newSopOperator(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator("sop_lsystem",                 // Internal name
                                       "L-System SOP",                // UI name
                                       SOP_Lsystem::nodeConstructor,  // How to build the SOP
                                       SOP_Lsystem::templateList,     // parameters
                                       0,                             // Min inputs
                                       0,                             // Max inputs
                                       SOP_Lsystem::localVariables,   // Local variables
                                       OP_FLAG_GENERATOR)             // Flag it as generator
    );
}

static PRM_Name angleName("angle", "Angle");
static PRM_Default angleDefault(22.5f);

static PRM_Name stepSizeName("stepSize", "Step Size");
static PRM_Default stepSizeDefault(1.5f);

static PRM_Name iterationsName("iterations", "Iterations");
static PRM_Default iterationsDefault(5);

static PRM_Name grammarFileName("grammarFile", "Grammar File");
static PRM_Default grammarFileDefault(0.f, "");
static PRM_SpareData grammarFileSpareData(
    PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.txt")
                    << PRM_SpareToken(PRM_SpareData::getFileChooserModeToken(), "read"));
constexpr char grammarFileHelpText[] = "Overrides 'Grammar String' if set";

static PRM_Name grammarStringName("grammarString", "Grammar String");
static PRM_Default grammarStringDefault(0.f, "F\nF->F[+F]F[-F]F");
constexpr char grammarStringHelpText[] = "Overriden by 'Grammar File' if set";

static PRM_Name radiusName("radius", "Radius");
static PRM_Default radiusDefault(0.1f);
static PRM_Range radiusRange(PRM_RangeFlag::PRM_RANGE_RESTRICTED, 0.01f,
                             PRM_RangeFlag::PRM_RANGE_UI, 10.f);

static PRM_Name divisionsName("divisions", "Divisions");
static PRM_Default divisionsDefault(6);
static PRM_Range divisionsRange(PRM_RangeFlag::PRM_RANGE_RESTRICTED, 4, PRM_RangeFlag::PRM_RANGE_UI,
                                30);

PRM_Template SOP_Lsystem::templateList[] = {
    PRM_Template(PRM_FLT, PRM_Template::PRM_EXPORT_MIN, 1, &angleName, &angleDefault, 0,
                 &PRMhalfAngleRange),
    PRM_Template(PRM_FLT, PRM_Template::PRM_EXPORT_MIN, 1, &stepSizeName, &stepSizeDefault, 0,
                 &PRMstepSizeRange),
    PRM_Template(PRM_INT, PRM_Template::PRM_EXPORT_MIN, 1, &iterationsName, &iterationsDefault, 0,
                 &PRMfrequency10Range),
    PRM_Template(PRM_FLT, PRM_Template::PRM_EXPORT_MIN, 1, &radiusName, &radiusDefault, 0,
                 &radiusRange),
    PRM_Template(PRM_INT, PRM_Template::PRM_EXPORT_MIN, 1, &divisionsName, &divisionsDefault, 0,
                 &divisionsRange),
    PRM_Template(PRM_FILE, PRM_Template::PRM_EXPORT_MIN, 1, &grammarFileName, &grammarFileDefault,
                 0, 0, 0, &grammarFileSpareData, 1, grammarFileHelpText),
    PRM_Template(PRM_STRING, PRM_Template::PRM_EXPORT_MIN, 1, &grammarStringName,
                 &grammarStringDefault, 0, 0, 0, 0, 1, grammarStringHelpText),
    PRM_Template()  // a sentinel template is needed to signal end of list
};

// Here's how we define local variables for the SOP.
enum
{
    VAR_PT,  // Point number of the star
    VAR_NPT  // Number of points in the star
};

CH_LocalVariable SOP_Lsystem::localVariables[] = {
    { "PT", VAR_PT, 0 },
    { "NPT", VAR_NPT, 0 },
    { 0, 0, 0 },
};

SOP_Lsystem::SOP_Lsystem(OP_Network* net, const char* name, OP_Operator* op)
    : SOP_Node(net, name, op), currPoint(UINT32_MAX), totalPoints(UINT32_MAX)
{
}

SOP_Lsystem::~SOP_Lsystem() {}

bool SOP_Lsystem::evalVariableValue(fpreal& val, int index, int thread)
{
    // handle the local variables when we have a valid `currPoint` index.
    if (currPoint < UINT32_MAX)
    {
        // Note that "gdp" may be null here, so we do the safe thing
        // and cache values we are interested in.
        switch (index)
        {
            case VAR_NPT: val = static_cast<fpreal>(totalPoints); return true;
            default: break;
        }
    }
    // Not one of our variables, must delegate to the base class.
    return SOP_Node::evalVariableValue(val, index, thread);
}

OP_Node* SOP_Lsystem::nodeConstructor(OP_Network* net, const char* name, OP_Operator* op)
{
    return new SOP_Lsystem(net, name, op);
}

unsigned SOP_Lsystem::disableParms()
{
    return 0;
}

OP_ERROR SOP_Lsystem::parseGrammar(OP_Context& context, std::string* outGrammar)
{
    fpreal now = context.getTime();

    UT_String grammarFile = GRAMMARFILE(now);  // `evalString` expands all variables for us
    if (grammarFile.isstring())
    {
        FS_Reader reader(grammarFile);
        if (!reader.isGood())
        {
            addError(SOP_MESSAGE, "Failed to parse 'Grammar File' for grammar.");
            return error(context);
        }
        UT_IStream* stream = reader.getStream();
        if (!stream)
        {
            // HDK indicates this would be the reason for a nullptr
            addError(SOP_MESSAGE, "'Grammar File' could not be found.");
            return error(context);
        }

        UT_WorkBuffer grammarBuffer;
        // converts line-endings to '\n', which `L-System` parses for
        bool result = stream->getAllAscii(grammarBuffer);
        *outGrammar = grammarBuffer.toStdString();

        reader.close();

        if (result)
        {
            return error(context);  // was able to parse file for grammar
        }
    }
    UT_String grammarString = GRAMMARSTRING(now);
    if (!grammarString.isstring())
    {
        // string is empty, which is technically not an error that requires a message
        return error(context);
    }
    *outGrammar = grammarString.toStdString();
    return error(context);
}

OP_ERROR SOP_Lsystem::cookMySop(OP_Context& context)
{
    fpreal now = context.getTime();

    std::string grammarString = "";
    OP_ERROR err = this->parseGrammar(context, &grammarString);
    if (grammarString.empty())
    {
        return err;
    }

    // initialize and construct `LSystem`
    LSystem system;
    system.loadProgramFromString(grammarString);
    system.setDefaultAngle(ANGLE(now));
    system.setDefaultStep(STEPSIZE(now));

    size_t iterations = ITERATIONS(now);

    std::vector<LSystem::Branch> branches;

    for (size_t i = 0; i < iterations; i++)
    {
        system.process(i, branches);
    }

    float radius = RADIUS(now);
    uint32_t divisions = DIVISIONS(now);

    uint32_t branchesSize = CLAMP_UINT64_TO_UINT32(branches.size());

    // set the NPT local variable value
    totalPoints = 2 * divisions * branchesSize;

    currPoint = 0;  // begin updating the PT local variable

    UT_Interrupt* boss;

    boss = UTgetInterrupt();
    gdp->clearAndDestroy();

    // operate within the interrupt context
    if (!boss->opStart("Building L-System"))
    {
        boss->opEnd();
        return error(context);
    }

    UT_Vector3 start, end, axis, up, tangent, bitangent;
    UT_Vector3 radial, bottomPos, topPos;  // inner loop vectors

    GU_PrimPoly* poly;
    std::vector<GA_Offset> bottomRing(divisions);
    std::vector<GA_Offset> topRing(divisions);

    // use the start and end point of each point to render these branches into Houdini
    for (uint32_t i = 0; i < branchesSize; i++)
    {
        if (boss->opInterrupt())
        {
            break;
        }
        currPoint = i;
        const LSystem::Branch& branch = branches[i];

        start = UT_Vector3(branch.first[VX], branch.first[VY], branch.first[VZ]);
        end = UT_Vector3(branch.second[VX], branch.second[VY], branch.second[VZ]);

        axis = end - start;
        float height = axis.length();
        if (height <= FLT_EPSILON)
        {
            continue;
        }
        axis.normalize();

        // build orthonormal bases
        up = UT_Vector3(0, 1, 0);
        if (SYSabs(axis.dot(up)) > 1.f - FLT_EPSILON)  // axes are parallel
        {
            up = UT_Vector3(1, 0, 0);
        }

        tangent = cross(axis, up);
        tangent.normalize();

        bitangent = cross(axis, tangent);
        bitangent.normalize();

        for (uint32_t j = 0; j < divisions; ++j)
        {
            float angle = (2.0f * M_PI * j) / divisions;
            float cosA = cosf(angle);
            float sinA = sinf(angle);

            // the radial hypotenuse vector for the current angle
            radial = (tangent * cosA + bitangent * sinA) * radius;

            bottomPos = start + radial;
            topPos = end + radial;

            bottomRing[j] = gdp->appendPointOffset();
            topRing[j] = gdp->appendPointOffset();

            gdp->setPos3(bottomRing[j], bottomPos);
            gdp->setPos3(topRing[j], topPos);
        }

        // connect rings with quad faces
        for (uint32_t j = 0; j < divisions; j++)
        {
            uint32_t next = (j + 1) % divisions;  // wrap around

            poly = GU_PrimPoly::build(gdp, 4, GU_POLY_CLOSED);
            if (!poly)
            {
                continue;
            }

            poly->setPointOffset(0, bottomRing[j]);
            poly->setPointOffset(1, bottomRing[next]);
            poly->setPointOffset(2, topRing[next]);
            poly->setPointOffset(3, topRing[j]);
        }
    }

    // clears any currently highlighted prims and selects prims from this SOP
    select(GA_GroupType::GA_GROUP_PRIMITIVE);

    // tell the interrupt server that we've completed.
    // must do this regardless of what opStart() returns.
    boss->opEnd();

    currPoint = UINT32_MAX;
    return error(context);
}
