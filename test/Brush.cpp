#include "RadiantTest.h"

#include "ibrush.h"
#include "imap.h"
#include "iselection.h"
#include "itransformable.h"
#include "scenelib.h"
#include "math/Quaternion.h"
#include "algorithm/Scene.h"
#include "algorithm/Primitives.h"
#include "math/Vector3.h"
#include "os/path.h"
#include "testutil/FileSelectionHelper.h"

namespace test
{

// Fuzzy equality assertion for Plane3
void expectNear(const Plane3& p1, const Plane3& p2, double epsilon)
{
    EXPECT_NEAR(p1.normal().x(), p2.normal().x(), epsilon);
    EXPECT_NEAR(p1.normal().y(), p2.normal().y(), epsilon);
    EXPECT_NEAR(p1.normal().z(), p2.normal().z(), epsilon);
    EXPECT_NEAR(p1.dist(), p2.dist(), epsilon);
}

inline bool faceHasVertex(const IFace* face, const Vector3& expectedXYZ, const Vector2& expectedUV)
{
    return algorithm::faceHasVertex(face, [&](const WindingVertex& vertex)
    {
        return math::isNear(vertex.vertex, expectedXYZ, 0.01) && math::isNear(vertex.texcoord, expectedUV, 0.01);
    });
}

using Quake3BrushTest = RadiantTest;

class BrushTest: public RadiantTest
{
protected:
    scene::INodePtr _brushNode;

    void testFacePlane(const std::function<bool(const IBrushNodePtr&)>& functor)
    {
        auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

        _brushNode = GlobalBrushCreator().createBrush();
        worldspawn->addChildNode(_brushNode);

        GlobalSelectionSystem().setSelectedAll(false);
        Node_setSelected(_brushNode, true);

        const double size = 15;
        Vector3 startPos(-size, -size, -size);
        Vector3 endPos(size, size, size);
        GlobalCommandSystem().executeCommand("ResizeSelectedBrushesToBounds",
            startPos, endPos, std::string("shader"));

        auto result = functor(std::dynamic_pointer_cast<IBrushNode>(_brushNode));

        scene::removeNodeFromParent(_brushNode);
        _brushNode.reset();

        EXPECT_TRUE(result) << "Test failed to perform anything.";
    }
};

inline bool isSane(const Matrix4& matrix)
{
    for (std::size_t i = 0; i < 15; ++i)
    {
        if (std::isnan(matrix[i]) || std::isinf(matrix[i]))
        {
            return false;
        }
    }

    return true;
}

TEST_F(BrushTest, FitTextureWithZeroScale)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    auto brushNode = GlobalBrushCreator().createBrush();
    worldspawn->addChildNode(brushNode);

    GlobalSelectionSystem().setSelectedAll(false);
    Node_setSelected(brushNode, true);

    const double size = 15;
    Vector3 startPos(-size, -size, -size);
    Vector3 endPos(size, size, size);
    GlobalCommandSystem().executeCommand("ResizeSelectedBrushesToBounds",
        startPos, endPos, std::string("shader"));

    GlobalSelectionSystem().setSelectedAll(false);

    auto brush = std::dynamic_pointer_cast<IBrushNode>(brushNode);
    brush->getIBrush().evaluateBRep();

    // Apply a texdef with a 0 scale component to the first face
    ShiftScaleRotation scr;

    scr.shift[0] = 0.0f;
    scr.shift[1] = 0.0f;
    scr.scale[0] = 1.0f;
    scr.scale[1] = 0.0f; // zero scale
    scr.rotate = 0.0f;

    auto& face = brush->getIBrush().getFace(0);

    face.setShiftScaleRotation(scr);

    auto matrix = face.getProjectionMatrix();
    EXPECT_FALSE(isSane(matrix)); // 5th matrix component is INF at the least

    // Now fit the texture
    face.fitTexture(1, 1);

    matrix = face.getProjectionMatrix();

    // The whole matrix should be sane now
    EXPECT_TRUE(isSane(matrix)) << "Texture Projection Matrix is not sane after fitting";

    scene::removeNodeFromParent(brushNode);
}

TEST_F(BrushTest, FacePlaneRotateWithMatrix)
{
    testFacePlane([this](const IBrushNodePtr& brush)
    {
        // Get the plane facing down the x axis and check it
        for (std::size_t i = 0; i < brush->getIBrush().getNumFaces(); ++i)
        {
            auto& face = brush->getIBrush().getFace(i);

            if (math::isParallel(face.getPlane3().normal(), Vector3(1, 0, 0)))
            {
                Plane3 orig = face.getPlane3();

                // Transform the plane with a rotation matrix
                const double ANGLE = 2.0;
                Matrix4 rot = Matrix4::getRotation(Vector3(0, 1, 0), ANGLE);

                Node_getTransformable(_brushNode)->setRotation(
                    Quaternion::createForY(-ANGLE)
                );
                Node_getTransformable(_brushNode)->freezeTransform();

                double EPSILON = 0.001;
                EXPECT_NE(face.getPlane3(), orig);
                expectNear(face.getPlane3(), orig.transformed(rot), EPSILON);
                EXPECT_NEAR(face.getPlane3().normal().getLength(), 1, EPSILON);

                return true;
            }
        }

        return false;
    });
}

TEST_F(BrushTest, FacePlaneTranslate)
{
    testFacePlane([this](const IBrushNodePtr& brush)
    {
        // Get the plane facing down the x axis and check it
        for (std::size_t i = 0; i < brush->getIBrush().getNumFaces(); ++i)
        {
            auto& face = brush->getIBrush().getFace(i);

            if (math::isParallel(face.getPlane3().normal(), Vector3(0, 1, 0)))
            {
                Plane3 orig = face.getPlane3();

                // Translate in the Y direction
                const Vector3 translation(0, 3, 0);

                Node_getTransformable(_brushNode)->setTranslation(translation);
                Node_getTransformable(_brushNode)->freezeTransform();

                EXPECT_NE(face.getPlane3(), orig);
                EXPECT_EQ(face.getPlane3().normal(), orig.normal());
                EXPECT_EQ(face.getPlane3().dist(), orig.dist() + translation.y());
                EXPECT_NEAR(face.getPlane3().normal().getLength(), 1, 0.001);

                return true;
            }
        }

        return false;
    });
}

TEST_F(BrushTest, FaceRotateTexDef)
{
    std::string mapPath = "maps/simple_brushes.map";
    GlobalCommandSystem().executeCommand("OpenMap", mapPath);

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();

    // Find the brush that is centered at origin
    auto brushNode = algorithm::findFirstBrushWithMaterial(worldspawn, "textures/numbers/2");
    EXPECT_TRUE(brushNode && brushNode->getNodeType() == scene::INode::Type::Brush) << "Couldn't locate the test brush";

    // Pick a few faces and run the algorithm against it, checking hardcoded results

    // Facing 0,0,1
    auto face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, 0, 1));

    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -160), Vector2(0, 1)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -160), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, 64, -160), Vector2(1, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -160), Vector2(1, 1)));

    face->rotateTexdef(15); // degrees

    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -160), Vector2(-0.112372, 0.853553)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -160), Vector2(0.146447, -0.112372)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, 64, -160), Vector2(1.11237, 0.146447)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -160), Vector2(0.853553, 1.11237)));

    // Facing 1,0,0
    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(1, 0, 0));

    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -288), Vector2(0, 65)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -160), Vector2(0, 64)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -160), Vector2(1, 64)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -288), Vector2(1, 65)));
    
    face->rotateTexdef(15); // degrees
    
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -288), Vector2(-0.112372, 64.8536)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -160), Vector2(0.146447, 63.8876)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -160), Vector2(1.11237, 64.1464)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -288), Vector2(0.853553, 65.1124)));

    // Facing 0,0,-1
    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, 0, -1));

    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -288), Vector2(0, 1)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -288), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -288), Vector2(1, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, 64, -288), Vector2(1, 1)));
    
    face->rotateTexdef(15); // degrees

    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -288), Vector2(-0.112372, 0.853553)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -288), Vector2(0.146447, -0.112372)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, -288), Vector2(1.11237, 0.146447)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, 64, -288), Vector2(0.853553, 1.11237)));

    // Facing 0,-1,0, this time rotate -15 degrees
    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, -1, 0));

    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -160), Vector2(0, 64)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -160), Vector2(1, 64)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -288), Vector2(1, 65)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -288), Vector2(0, 65)));

    face->rotateTexdef(-15); // degrees

    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -160), Vector2(-0.112372, 64.1464)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -160), Vector2(0.853553, 63.8876)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, -64, -288), Vector2(1.11237, 64.8536)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-64, -64, -288), Vector2(0.146447, 65.1124)));
}

// Load a brush with one vertex at 0,0,0, and an identity shift/scale/rotation texdef
TEST_F(Quake3BrushTest, LoadBrushWithIdentityTexDef)
{
    std::string mapPath = "maps/quake3maps/brush_no_transform.map";
    GlobalCommandSystem().executeCommand("OpenMap", mapPath);

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    EXPECT_EQ(algorithm::getChildCount(worldspawn), 1) << "Scene has not exactly 1 brush";

    // Check that we have exactly one brush loaded
    auto brushNode = algorithm::findFirstBrushWithMaterial(worldspawn, "textures/a_1024x512");
    EXPECT_TRUE(brushNode && brushNode->getNodeType() == scene::INode::Type::Brush) << "Couldn't locate the test brush";

    auto face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, 0, 1));
    EXPECT_TRUE(face != nullptr) << "No brush plane is facing upwards?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(64,  0, 64), Vector2(0.0625, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3( 0,  0, 64), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3( 0, 64, 64), Vector2(0, -0.125)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, 64), Vector2(0.0625, -0.125)));

    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, 0, -1));
    EXPECT_TRUE(face != nullptr) << "No brush plane is facing down?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 0, 0), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 0, 0), Vector2(0.0625, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, 0), Vector2(0.0625, -0.125)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 64, 0), Vector2(0, -0.125)));

    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, -1, 0));
    EXPECT_TRUE(face != nullptr) << "No brush plane with normal 0,-1,0?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 0, 0), Vector2(0.0625, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 0, 0), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 0, 64), Vector2(0, -0.125)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 0, 64), Vector2(0.0625, -0.125)));

    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, 1, 0));
    EXPECT_TRUE(face != nullptr) << "No brush plane with normal 0,1,0?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 64, 0), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, 0), Vector2(0.0625, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, 64), Vector2(0.0625, -0.125)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 64, 64), Vector2(0, -0.125)));

    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(1, 0, 0));
    EXPECT_TRUE(face != nullptr) << "No brush plane with normal 1,0,0?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, 0), Vector2(0.0625, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 0, 0), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 0, 64), Vector2(0, -0.125)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(64, 64, 64), Vector2(0.0625, -0.125)));

    face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(-1, 0, 0));
    EXPECT_TRUE(face != nullptr) << "No brush plane with normal -1,0,0?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 0, 0), Vector2(0, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 64, 0), Vector2(0.0625, 0)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 64, 64), Vector2(0.0625, -0.125)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(0, 0, 64), Vector2(0, -0.125)));
}

// Load an axis-aligned brush somewhere at (-600 1000 56) and some shift/scale/rotation values
TEST_F(Quake3BrushTest, LoadAxisAlignedBrushWithTransform)
{
    std::string mapPath = "maps/quake3maps/brush_with_transform.map";
    GlobalCommandSystem().executeCommand("OpenMap", mapPath);

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    EXPECT_EQ(algorithm::getChildCount(worldspawn), 1) << "Scene has not exactly 1 brush";

    // Check that we have exactly one brush loaded
    auto brushNode = algorithm::findFirstBrushWithMaterial(worldspawn, "textures/a_1024x512");
    EXPECT_TRUE(brushNode && brushNode->getNodeType() == scene::INode::Type::Brush) << "Couldn't locate the test brush";

    auto face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, 0, 1));
    EXPECT_TRUE(face != nullptr) << "No brush plane is facing upwards?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(-624, 800, 64), Vector2(5, 13)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-688, 800, 64), Vector2(5.5, 13)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-688, 1024, 64), Vector2(5.5, 16.5)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-624, 1024, 64), Vector2(5, 16.5)));
}

// This loads the same brush as in the LoadAxisAlignedBrushWithTransform test
// and stores it again using the Quake3 brush format
TEST_F(Quake3BrushTest, SaveAxisAlignedBrushWithTransform)
{
    std::string mapPath = "maps/quake3maps/brush_with_transform.map";
    GlobalCommandSystem().executeCommand("OpenMap", mapPath);

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    EXPECT_EQ(algorithm::getChildCount(worldspawn), 1) << "Scene has not exactly 1 brush";

    fs::path tempPath = _context.getTemporaryDataPath();
    tempPath /= "brushexport.map";

    auto format = GlobalMapFormatManager().getMapFormatForGameType("quake3", os::getExtension(tempPath.string()));

    EXPECT_FALSE(fs::exists(tempPath)) << "File already exists: " << tempPath;

    FileSelectionHelper helper(tempPath.string(), format);
    GlobalCommandSystem().executeCommand("ExportMap");

    EXPECT_TRUE(fs::exists(tempPath)) << "File still doesn't exist: " << tempPath;

    std::ifstream tempFile(tempPath);
    std::stringstream content;
    content << tempFile.rdbuf();

    auto savedContent = content.str();

    // This is checking the actual face string as written by the LegacyBrushDefExporter
    // including whitespace and all the syntax details
    // The incoming brush had a rotation of 180 degrees and a positive scale
    // the map exporter code will re-calculate that and spit out 0 rotation and a negative scale.
    // The incoming also had the plane points picked from the middle of the brush edge,
    // the DR exporter is using the winding vertices as points
    constexpr const char* const expectedBrushFace =
        "( -688 1024 64 ) ( -624 800 64 ) ( -688 800 64 ) a_1024x512 128 256 0 -0.125 -0.125 134217728 0 0";

    EXPECT_NE(savedContent.find(expectedBrushFace), std::string::npos) <<
        "Couldn't locate the brush face " << expectedBrushFace << "\n, Saved Content is:\n" << savedContent;
}

#if 0
// This test case is not working since the Q3 texture projection mechanics
// are different from idTech4, so DR cannot render angled faces as they would
// appear in a Q3 engine.
TEST_F(Quake3BrushTest, TextureOnAngledBrush)
{
    std::string mapPath = "maps/quake3maps/angled_brush.map";
    GlobalCommandSystem().executeCommand("OpenMap", mapPath);

    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    EXPECT_EQ(algorithm::getChildCount(worldspawn), 1) << "Scene has not exactly 1 brush";

    // Check that we have exactly one brush loaded
    auto brushNode = algorithm::findFirstBrushWithMaterial(worldspawn, "textures/a_1024x512");
    EXPECT_TRUE(brushNode && brushNode->getNodeType() == scene::INode::Type::Brush) << "Couldn't locate the test brush";

    auto face = algorithm::findBrushFaceWithNormal(Node_getIBrush(brushNode), Vector3(0, -0.351123, +0.936329));
    EXPECT_TRUE(face != nullptr) << "Couldn't find the upwards facing brush face?";

    EXPECT_TRUE(faceHasVertex(face, Vector3(-624, 1040, 64), Vector2(5, 16.5)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-688, 1040, 64), Vector2(5.5, 16.5)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-688, 1168, 112), Vector2(5.5, 18.5)));
    EXPECT_TRUE(faceHasVertex(face, Vector3(-624, 1168, 112), Vector2(5, 18.5)));
}
#endif

}
