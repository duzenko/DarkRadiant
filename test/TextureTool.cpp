#include "RadiantTest.h"

#include <set>
#include <sigc++/connection.h>
#include "imap.h"
#include "ipatch.h"
#include "iselectable.h"
#include "itexturetoolmodel.h"
#include "icommandsystem.h"
#include "iselection.h"
#include "scenelib.h"
#include "algorithm/Primitives.h"
#include "render/TextureToolView.h"
#include "selection/SelectionVolume.h"
#include "Rectangle.h"

namespace test
{

using TextureToolTest = RadiantTest;

inline std::size_t getTextureToolNodeCount()
{
    std::size_t count = 0;

    GlobalTextureToolSceneGraph().foreachNode([&](const textool::INode::Ptr& node)
    {
        ++count;
        return true;
    });

    return count;
}

inline textool::INode::Ptr getFirstTextureToolNode()
{
    textool::INode::Ptr returnValue;

    GlobalTextureToolSceneGraph().foreachNode([&](const textool::INode::Ptr& node)
    {
        returnValue = node;
        return false;
    });

    return returnValue;
}

std::vector<textool::INode::Ptr> getAllSelectedTextoolNodes()
{
    std::vector<textool::INode::Ptr> selectedNodes;

    GlobalTextureToolSelectionSystem().foreachSelectedNode([&](const textool::INode::Ptr& node)
    {
        selectedNodes.push_back(node);
        return true;
    });

    return selectedNodes;
}

std::vector<textool::INode::Ptr> getAllSelectedComponentNodes()
{
    std::vector<textool::INode::Ptr> selectedNodes;

    GlobalTextureToolSelectionSystem().foreachSelectedComponentNode([&](const textool::INode::Ptr& node)
    {
        selectedNodes.push_back(node);
        return true;
    });

    return selectedNodes;
}

class SelectionChangedCatcher
{
private:
    bool _signalFired;
    sigc::connection _connection;

public:
    SelectionChangedCatcher() :
        _signalFired(false)
    {
        _connection = GlobalTextureToolSelectionSystem().signal_selectionChanged().connect([this]
        {
            _signalFired = true;
        });
    }

    bool signalHasFired() const
    {
        return _signalFired;
    }

    void reset()
    {
        _signalFired = false;
    }

    ~SelectionChangedCatcher()
    {
        _connection.disconnect();
    }
};

// Checks that changing the regular scene selection will have an effect on the tex tool scene
TEST_F(TextureToolTest, SceneGraphObservesSelection)
{
    std::string material = "textures/numbers/1";
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush1 = algorithm::createCubicBrush(worldspawn, Vector3(0,0,0), material);
    auto brush2 = algorithm::createCubicBrush(worldspawn, Vector3(0,256,256), material);

    // Empty tex tool scenegraph on empty scene selection
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 0) << "Non-empty selection at startup";
    EXPECT_EQ(getTextureToolNodeCount(), 0) << "There shouldn't be any textool nodes when the scene is empty";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), "") << "Active material shoud be empty";

    Node_setSelected(brush1, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1) << "1 Brush must be selected";

    // We don't know how many tex tool nodes there are, but it should be more than 0
    auto nodeCount = getTextureToolNodeCount();
    EXPECT_GT(nodeCount, 0) << "There should be some tex tool nodes now";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), material) << "Active material mismatch";

    Node_setSelected(brush2, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 2) << "2 Brushes must be selected";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), material) << "Active material mismatch";

    // Should be even more now
    auto nodeCount2 = getTextureToolNodeCount();
    EXPECT_GT(nodeCount2, nodeCount) << "There should be more tex tool nodes now";

    GlobalSelectionSystem().setSelectedAll(false);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 0) << "Non-empty selection at shutdown";
    EXPECT_EQ(getTextureToolNodeCount(), 0) << "There shouldn't be any textool nodes when the scene is empty";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), "") << "Active material should be empty again";
}

TEST_F(TextureToolTest, SceneGraphNeedsUniqueShader)
{
    std::string material1 = "textures/numbers/1";
    std::string material2 = "textures/numbers/2";
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush1 = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), material1);
    auto brush2 = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), material2);

    Node_setSelected(brush1, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1) << "1 Brush must be selected";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), material1) << "Active material mismatch";

    // We don't know how many tex tool nodes there are, but it should be more than 0
    EXPECT_GT(getTextureToolNodeCount(), 0) << "There should be some tex tool nodes now";

    Node_setSelected(brush2, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 2) << "2 Brushes must be selected";

    EXPECT_EQ(getTextureToolNodeCount(), 0) << "There should be no nodes now, since the material is non unique";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), "") << "Active material mismatch";

    // Deselect brush 1, now only brush 2 is selected
    Node_setSelected(brush1, false);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1) << "1 Brush must be selected";
    EXPECT_GT(getTextureToolNodeCount(), 0) << "There should be some tex tool nodes again";
    EXPECT_EQ(GlobalTextureToolSceneGraph().getActiveMaterial(), material2) << "Active material mismatch";
}

TEST_F(TextureToolTest, SceneGraphRecognisesBrushes)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush1 = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), "textures/numbers/1");

    Node_setSelected(brush1, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1) << "1 Brush must be selected";

    // We don't know how many tex tool nodes there are, but it should be more than 0
    EXPECT_GT(getTextureToolNodeCount(), 0) << "There should be some tex tool nodes now";
}

TEST_F(TextureToolTest, SceneGraphRecognisesPatches)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto patch = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
    scene::addNodeToContainer(patch, worldspawn);

    Node_setSelected(patch, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1) << "1 patch must be selected";

    // We don't know how many tex tool nodes there are, but it should be more than 0
    EXPECT_GT(getTextureToolNodeCount(), 0) << "There should be some tex tool nodes now";
}

TEST_F(TextureToolTest, PatchNodeBounds)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto patchNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);

    auto patch = Node_getIPatch(patchNode);
    patch->setDims(3, 3);

    auto origin = Vector2(5.4, -78.3);
    auto step = 0.45;
    // Accumulate the test bounds
    AABB checkedBounds;

    for (auto col = 0; col < 3; ++col)
    {
        for (auto row = 0; row < 3; ++row)
        {
            auto& ctrl = patch->ctrlAt(row, col);

            ctrl.texcoord = Vector2(origin.x() + step * col, origin.y() + step * row);
            checkedBounds.includePoint({ ctrl.texcoord.x(), ctrl.texcoord.y(), 0 });
        }
    }

    scene::addNodeToContainer(patchNode, worldspawn);
    Node_setSelected(patchNode, true);

    auto node = getFirstTextureToolNode();
    EXPECT_TRUE(node) << "No texture tool node here";

    EXPECT_TRUE(math::isNear(node->localAABB().getOrigin(), checkedBounds.getOrigin(), 0.01)) << 
        "Bounds mismatch, got " << node->localAABB().getOrigin() << " instead of " << checkedBounds.getOrigin();
    EXPECT_TRUE(math::isNear(node->localAABB().getExtents(), checkedBounds.getExtents(), 0.01)) <<
        "Bounds mismatch, got " << node->localAABB().getExtents() << " instead of " << checkedBounds.getExtents();
}

TEST_F(TextureToolTest, ForeachSelectedNode)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush1 = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), "textures/numbers/1");
    auto brush2 = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");
    auto patchNode = GlobalPatchModule().createPatch(patch::PatchDefType::Def2);
    Node_getIPatch(patchNode)->setDims(3, 3);
    Node_getIPatch(patchNode)->setShader("textures/numbers/1");

    Node_setSelected(brush1, true);
    Node_setSelected(brush2, true);
    Node_setSelected(patchNode, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 3) << "3 items must be selected";

    // We don't know how many tex tool nodes there are, but it should be more than 0
    EXPECT_GT(getTextureToolNodeCount(), 0) << "There should be some tex tool nodes now";

    std::set<textool::INode::Ptr> selectedNodes;
    std::size_t i = 0;

    // Selected every odd node
    GlobalTextureToolSceneGraph().foreachNode([&](const textool::INode::Ptr& node)
    {
        if (++i % 2 == 1)
        {
            node->setSelected(true);
            selectedNodes.emplace(node);
        }

        return true;
    });

    std::size_t selectedCount = 0;
    GlobalTextureToolSelectionSystem().foreachSelectedNode([&](const textool::INode::Ptr& node)
    {
        ++selectedCount;
        EXPECT_TRUE(selectedNodes.count(node) > 0) << "Node shouldn't be selected";
        return true;
    });

    EXPECT_EQ(selectedCount, selectedNodes.size()) << "Selection count didn't match";
}

inline void foreachPatchVertex(const IPatch& patch, const std::function<void(const PatchControl&)>& functor)
{
    for (std::size_t col = 0; col < patch.getWidth(); ++col)
    {
        for (std::size_t row = 0; row < patch.getHeight(); ++row)
        {
            functor(patch.ctrlAt(row, col));
        }
    }
}

inline AABB getTextureSpaceBounds(const IPatch& patch)
{
    AABB bounds;

    foreachPatchVertex(patch, [&](const PatchControl& control)
    {
        const auto& uv = control.texcoord;
        bounds.includePoint({ uv.x(), uv.y(), 0 });
    });

    return bounds;
}

inline AABB getTextureSpaceBounds(const IFace& face)
{
    AABB bounds;

    for (const auto& vertex : face.getWinding())
    {
        bounds.includePoint({ vertex.texcoord.x(), vertex.texcoord.y(), 0 });
    }

    return bounds;
}

constexpr int TEXTOOL_WIDTH = 500;
constexpr int TEXTOOL_HEIGHT = 400;

inline scene::INodePtr setupPatchNodeForTextureTool()
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto patchNode = algorithm::createPatchFromBounds(worldspawn, AABB(Vector3(4, 50, 60), Vector3(64, 128, 256)), "textures/numbers/1");

    auto patch = Node_getIPatch(patchNode);
    patch->scaleTextureNaturally();
    patch->controlPointsChanged();

    // Select this node in the scene, to make it available in the texture tool
    Node_setSelected(patchNode, true);

    return patchNode;
}

inline textool::IFaceNode::Ptr findTexToolFaceWithNormal(const Vector3& normal)
{
    textool::IFaceNode::Ptr result;

    GlobalTextureToolSceneGraph().foreachNode([&](const textool::INode::Ptr& node)
    {
        auto faceNode = std::dynamic_pointer_cast<textool::IFaceNode>(node);

        if (faceNode && math::isNear(faceNode->getFace().getPlane3().normal(), normal, 0.01))
        {
            result = faceNode;
        }

        return result == nullptr;
    });

    return result;
}

// Default manipulator mode should be "Drag"
TEST_F(TextureToolTest, DefaultManipulatorMode)
{
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulatorType(), selection::IManipulator::Drag);
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulator()->getType(), selection::IManipulator::Drag);
}

TEST_F(TextureToolTest, DefaultSelectionMode)
{
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Surface);
}

TEST_F(TextureToolTest, ToggleManipulatorModes)
{
    // We're starting in "Drag" mode, so toggling the default mode should do nothing
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Drag" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulatorType(), selection::IManipulator::Drag);

    // Toggle to Rotate
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Rotate" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulatorType(), selection::IManipulator::Rotate);

    // Toggle from Rotate back to Drag
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Rotate" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulatorType(), selection::IManipulator::Drag);

    // Toggle to Rotate again
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Rotate" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulatorType(), selection::IManipulator::Rotate);

    // Toggle Drag explicitly
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Drag" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getActiveManipulatorType(), selection::IManipulator::Drag);
}

TEST_F(TextureToolTest, ManipulatorModeChangedSignal)
{
    bool signalFired = false;
    selection::IManipulator::Type signalArgument;

    // Subscribe to the changed signal
    sigc::connection conn = GlobalTextureToolSelectionSystem().signal_activeManipulatorChanged().connect(
        [&](selection::IManipulator::Type type)
    {
        signalFired = true;
        signalArgument = type;
    });

    // We're starting in drag mode, so no changed expected
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Drag" });
    EXPECT_FALSE(signalFired) << "Signal shouldn't have fired";
    signalFired = false;

    // Changing to Rotate should fire the signal
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Rotate" });
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    EXPECT_EQ(signalArgument, selection::IManipulator::Rotate) << "Signal communicated wrong mode";
    signalFired = false;

    // Toggle Rotate, should switch back to Drag
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Rotate" });
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    EXPECT_EQ(signalArgument, selection::IManipulator::Drag) << "Signal communicated wrong mode";
    signalFired = false;

    // Changing to Rotate (again) should fire the signal
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Rotate" });
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    EXPECT_EQ(signalArgument, selection::IManipulator::Rotate) << "Signal communicated wrong mode";
    signalFired = false;

    // Directly toggle to Drag, should fire
    GlobalCommandSystem().executeCommand("ToggleTextureToolManipulatorMode", { "Drag" });
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    EXPECT_EQ(signalArgument, selection::IManipulator::Drag) << "Signal communicated wrong mode";
    signalFired = false;

    conn.disconnect();
}

TEST_F(TextureToolTest, ToggleSelectionMode)
{
    bool signalFired = false;
    textool::SelectionMode signalArgument;

    // Subscribe to the changed signal
    sigc::connection conn = GlobalTextureToolSelectionSystem().signal_selectionModeChanged().connect(
        [&](textool::SelectionMode mode)
    {
        signalFired = true;
        signalArgument = mode;
    });

    // We're starting in Surface mode, toggle to Surface again
    GlobalCommandSystem().executeCommand("ToggleTextureToolSelectionMode", { "Surface" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Surface);
    EXPECT_FALSE(signalFired) << "Signal shouldn't have fired";
    signalFired = false;

    // Switch to vertex mode
    GlobalCommandSystem().executeCommand("ToggleTextureToolSelectionMode", { "Vertex" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Vertex);
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    signalFired = false;

    // Toggle vertex mode again => back to surface mode
    GlobalCommandSystem().executeCommand("ToggleTextureToolSelectionMode", { "Vertex" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Surface);
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    signalFired = false;

    // Switch to vertex mode (again)
    GlobalCommandSystem().executeCommand("ToggleTextureToolSelectionMode", { "Vertex" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Vertex);
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    signalFired = false;

    // Directly toggle surface mode
    GlobalCommandSystem().executeCommand("ToggleTextureToolSelectionMode", { "Surface" });
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Surface);
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    signalFired = false;
}

TEST_F(TextureToolTest, SelectionModeChangedSignal)
{
    bool signalFired = false;
    textool::SelectionMode signalArgument;

    // Subscribe to the changed signal
    sigc::connection conn = GlobalTextureToolSelectionSystem().signal_selectionModeChanged().connect(
        [&] (textool::SelectionMode mode)
        {
            signalFired = true;
            signalArgument = mode;
        });

    // We're starting in Surface mode, so no changed expected
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Surface);
    EXPECT_FALSE(signalFired) << "Signal shouldn't have fired";
    signalFired = false;

    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    EXPECT_EQ(signalArgument, textool::SelectionMode::Vertex) << "Signal communicated wrong mode";
    signalFired = false;

    // Switch to the same mode again => no signal expected
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);
    EXPECT_FALSE(signalFired) << "Signal shouldn't have fired";
    signalFired = false;

    // Back to surface mode
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Surface);
    EXPECT_TRUE(signalFired) << "Signal should have fired";
    EXPECT_EQ(signalArgument, textool::SelectionMode::Surface) << "Signal communicated wrong mode";
    signalFired = false;

    conn.disconnect();
}

void performPointSelection(const Vector2& texcoord, const render::View& view)
{
    auto texcoordTransformed = view.GetViewProjection().transformPoint(Vector3(texcoord.x(), texcoord.y(), 0));
    Vector2 devicePoint(texcoordTransformed.x(), texcoordTransformed.y());

    // Use the device point we calculated for this vertex and use it to construct a selection test
    render::View scissored(view);
    ConstructSelectionTest(scissored, selection::Rectangle::ConstructFromPoint(devicePoint, Vector2(0.02f, 0.02f)));

    SelectionVolume test(scissored);
    GlobalTextureToolSelectionSystem().selectPoint(test, SelectionSystem::eToggle);
}

TEST_F(TextureToolTest, TestSelectPatchSurfaceByPoint)
{
    auto patchNode = setupPatchNodeForTextureTool();
    auto patch = Node_getIPatch(patchNode);

    // Get the texture space bounds of this patch
    auto bounds = getTextureSpaceBounds(*patch);

    // Construct a view that includes the patch UV bounds
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    SelectionChangedCatcher signalObserver;

    // Test-select in the middle of the patch bounds
    performPointSelection(Vector2(bounds.origin.x(), bounds.origin.y()), view);

    EXPECT_TRUE(signalObserver.signalHasFired()) << "No selection changed signal emitted";

    // Check if the node was selected
    auto selectedNodes = getAllSelectedTextoolNodes();
    EXPECT_EQ(selectedNodes.size(), 1) << "Only one patch should be selected";
    EXPECT_TRUE(std::dynamic_pointer_cast<textool::IPatchNode>(selectedNodes.front())) << "Couldn't cast to special type";
}

TEST_F(TextureToolTest, TestSelectPatchVertexByPoint)
{
    auto patchNode = setupPatchNodeForTextureTool();
    auto patch = Node_getIPatch(patchNode);

    // Get the texture space bounds of this patch
    auto bounds = getTextureSpaceBounds(*patch);

    // Construct a view that includes the patch UV bounds
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Switch to vertex selection mode
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    // Get the texcoords of the first vertex
    auto firstVertex = patch->ctrlAt(2, 1).texcoord;
    auto secondVertex = patch->ctrlAt(2, 0).texcoord;

    SelectionChangedCatcher signalObserver;

    // Selecting something in the middle of two vertices should not do anything
    performPointSelection((firstVertex + secondVertex) / 2, view);
    EXPECT_TRUE(getAllSelectedComponentNodes().empty()) << "Test-selecting a patch in between vertices should not have succeeded";
    EXPECT_FALSE(signalObserver.signalHasFired()) << "Selection Changed Signal shouldn't have fired";
    signalObserver.reset();

    performPointSelection(firstVertex, view);

    // Hitting a vertex will select the patch componentselectable
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one patch should be selected";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    // Hitting another vertex should not de-select the componentselectable
    performPointSelection(secondVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one patch should still be selected";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    // De-selecting the first and the second vertex should release the patch
    performPointSelection(secondVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one patch should still be selected";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    performPointSelection(firstVertex, view);
    EXPECT_TRUE(getAllSelectedComponentNodes().empty()) << "Selection should be empty now";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();
}

TEST_F(TextureToolTest, TestSelectFaceSurfaceByPoint)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");
    scene::addNodeToContainer(brush, worldspawn);

    // Put all faces into the tex tool scene
    Node_setSelected(brush, true);

    auto faceUp = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush), Vector3(0, 0, 1));

    // Check the face
    auto textoolFace = findTexToolFaceWithNormal(faceUp->getPlane3().normal());
    EXPECT_FALSE(textoolFace->isSelected()) << "Face should be unselected at start";

    // Get the texture space bounds of this face
    // Construct a view that includes the patch UV bounds
    auto bounds = getTextureSpaceBounds(*faceUp);
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    SelectionChangedCatcher signalObserver;

    // Point-select in the middle of the face
    performPointSelection(algorithm::getFaceCentroid(faceUp), view);

    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";

    // Check if the node was selected
    auto selectedNodes = getAllSelectedTextoolNodes();
    EXPECT_EQ(selectedNodes.size(), 1) << "Only one item should be selected";
    EXPECT_EQ(selectedNodes.front(), textoolFace) << "The face should be selected";
    EXPECT_TRUE(std::dynamic_pointer_cast<textool::IFaceNode>(selectedNodes.front())) << "Couldn't cast to special type";
}

TEST_F(TextureToolTest, TestSelectFaceVertexByPoint)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");
    scene::addNodeToContainer(brush, worldspawn);

    // Put all faces into the tex tool scene
    Node_setSelected(brush, true);

    auto faceUp = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush), Vector3(0, 0, 1));

    // Get the texture space bounds of this face
    auto bounds = getTextureSpaceBounds(*faceUp);

    // Construct a view that includes the patch UV bounds
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Switch to vertex selection mode
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    SelectionChangedCatcher signalObserver;

    // Get the texcoords of the first vertex
    auto firstVertex = faceUp->getWinding()[0].texcoord;
    auto secondVertex = faceUp->getWinding()[1].texcoord;

    // Selecting something in the middle of two vertices should not do anything
    performPointSelection((firstVertex + secondVertex) / 2, view);
    EXPECT_TRUE(getAllSelectedComponentNodes().empty()) << "Test-selecting a face in between vertices should not have succeeded";
    EXPECT_FALSE(signalObserver.signalHasFired()) << "Selection Changed Signal shouldn't have fired";
    signalObserver.reset();

    // Hitting a vertex will select the face
    performPointSelection(firstVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one face should be selected";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    // Hitting another vertex should not de-select the face
    performPointSelection(secondVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one face should still be selected";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    // De-selecting the first and the second vertex should release the face
    performPointSelection(secondVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one face should still be selected";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    performPointSelection(firstVertex, view);
    EXPECT_TRUE(getAllSelectedComponentNodes().empty()) << "Selection should be empty now";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();
}

TEST_F(TextureToolTest, TestSelectPatchByArea)
{
    auto patchNode = setupPatchNodeForTextureTool();
    auto patch = Node_getIPatch(patchNode);

    // Get the texture space bounds of this patch
    auto bounds = getTextureSpaceBounds(*patch);

    // Construct a view that includes the patch UV bounds
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Use the device point we calculated for this vertex and use it to construct a selection test
    ConstructSelectionTest(view, selection::Rectangle::ConstructFromArea(Vector2(-0.95f, -0.95f), Vector2(0.95f*2, 0.95f*2)));

    SelectionVolume test(view);
    SelectionChangedCatcher signalObserver;

    GlobalTextureToolSelectionSystem().selectArea(test, SelectionSystem::eToggle);

    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";

    // Check if the node was selected
    auto selectedNodes = getAllSelectedTextoolNodes();
    EXPECT_EQ(selectedNodes.size(), 1) << "Only one patch should be selected";
    EXPECT_TRUE(std::dynamic_pointer_cast<textool::IPatchNode>(selectedNodes.front())) << "Couldn't cast to special type";
}

TEST_F(TextureToolTest, ClearSelectionUsingCommand)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush1 = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), "textures/numbers/1");
    auto brush2 = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");
    auto patchNode = setupPatchNodeForTextureTool();
    Node_getIPatch(patchNode)->setShader("textures/numbers/1");

    Node_setSelected(brush1, true);
    Node_setSelected(brush2, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 3) << "3 items must be selected";

    // We don't know how many tex tool nodes there are, but it should be more than 0
    EXPECT_GT(getTextureToolNodeCount(), 0) << "There should be some tex tool nodes now";

    std::set<textool::INode::Ptr> selectedNodes;
    std::size_t i = 0;

    // Select every single node
    GlobalTextureToolSceneGraph().foreachNode([&](const textool::INode::Ptr& node)
    {
        node->setSelected(true);
        selectedNodes.emplace(node);
        return true;
    });

    // We should have a non-empty selection
    EXPECT_GT(GlobalTextureToolSelectionSystem().countSelected(), 0) << "No nodes selected";

    // Switch to vertex mode
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    // Get the texture space bounds of this patch
    render::TextureToolView view;
    auto bounds = getTextureSpaceBounds(*Node_getIPatch(patchNode));
    bounds.extents *= 1.2f;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Select patch vertices
    foreachPatchVertex(*Node_getIPatch(patchNode), [&](const PatchControl& control)
    {
        performPointSelection(control.texcoord, view);
    });

    // Select face vertices
    auto faceUp = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush1), Vector3(0, 0, 1));

    // Get the texture space bounds of this face
    bounds = getTextureSpaceBounds(*faceUp);
    bounds.extents *= 1.2f;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    for (const auto& vertex : faceUp->getWinding())
    {
        performPointSelection(vertex.texcoord, view);
    }

    // We should have two selected component nodes
    EXPECT_GT(GlobalTextureToolSelectionSystem().countSelectedComponentNodes(), 0) << "No components selected";
    EXPECT_GT(GlobalSelectionSystem().countSelected(), 0) << "Scene selection count should be > 0";

    SelectionChangedCatcher signalObserver;

    // Hitting ESC once will deselect the components
    GlobalCommandSystem().executeCommand("UnSelectSelection");

    EXPECT_EQ(GlobalTextureToolSelectionSystem().countSelectedComponentNodes(), 0) << "Component selection should be gone";
    EXPECT_GT(GlobalTextureToolSelectionSystem().countSelected(), 0) << "Surface selection should not have been touched";
    EXPECT_GT(GlobalSelectionSystem().countSelected(), 0) << "Scene selection count should still be > 0";
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Vertex) << "We should still be in vertex mode";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    // Next deselection will exit vertex mode
    GlobalCommandSystem().executeCommand("UnSelectSelection");
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Surface) << "We should be in Surface mode now";
    EXPECT_GT(GlobalTextureToolSelectionSystem().countSelected(), 0) << "Surface selection should not have been touched";
    EXPECT_GT(GlobalSelectionSystem().countSelected(), 0) << "Scene selection count should still be > 0";
    EXPECT_FALSE(signalObserver.signalHasFired()) << "Selection Changed Signal shouldn't have fired";
    signalObserver.reset();

    // Next will de-select the regular selection
    GlobalCommandSystem().executeCommand("UnSelectSelection");
    EXPECT_EQ(GlobalTextureToolSelectionSystem().countSelected(), 0) << "Surface selection should be gone now";
    EXPECT_GT(GlobalSelectionSystem().countSelected(), 0) << "Scene selection count should still be > 0";
    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    signalObserver.reset();

    // Now that the tex tool selection is gone, we should affect the scene selection
    GlobalCommandSystem().executeCommand("UnSelectSelection");
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 0) << "Scene selection should be gone now";
    EXPECT_FALSE(signalObserver.signalHasFired()) << "Selection Changed Signal shouldn't have fired";
}

TEST_F(TextureToolTest, ClearSelection)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush1 = algorithm::createCubicBrush(worldspawn, Vector3(0, 0, 0), "textures/numbers/1");
    auto brush2 = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");
    auto patchNode = setupPatchNodeForTextureTool();
    Node_getIPatch(patchNode)->setShader("textures/numbers/1");

    Node_setSelected(brush1, true);
    Node_setSelected(brush2, true);
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 3) << "3 scene nodes must be selected";

    std::set<textool::INode::Ptr> selectedNodes;
    std::size_t i = 0;

    // Select every single node
    GlobalTextureToolSceneGraph().foreachNode([&](const textool::INode::Ptr& node)
    {
        node->setSelected(true);
        selectedNodes.emplace(node);
        return true;
    });

    // We should have a non-empty selection
    EXPECT_GT(GlobalTextureToolSelectionSystem().countSelected(), 0) << "No nodes selected";

    SelectionChangedCatcher signalObserver;

    // Deselect
    GlobalTextureToolSelectionSystem().clearSelection();

    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";

    EXPECT_EQ(GlobalTextureToolSelectionSystem().countSelected(), 0) << "Surface selection should be gone now";
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 3) << "3 scene nodes must be selected";

    for (const auto& node : selectedNodes)
    {
        EXPECT_FALSE(node->isSelected()) << "Node should have been deselected";
    }
}

TEST_F(TextureToolTest, ClearComponentSelection)
{
    auto patchNode = setupPatchNodeForTextureTool();
    EXPECT_EQ(GlobalSelectionSystem().countSelected(), 1) << "1 scene node must be selected";

    // Switch to vertex mode
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    // Get the texture space bounds of this patch
    render::TextureToolView view;
    auto bounds = getTextureSpaceBounds(*Node_getIPatch(patchNode));
    bounds.extents *= 1.2f;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Select patch vertices
    foreachPatchVertex(*Node_getIPatch(patchNode), [&](const PatchControl& control)
    {
        performPointSelection(control.texcoord, view);
    });

    EXPECT_EQ(GlobalTextureToolSelectionSystem().countSelectedComponentNodes(), 1) << "We should have 1 selected component node";

    SelectionChangedCatcher signalObserver;

    // Deselect all components
    GlobalTextureToolSelectionSystem().clearComponentSelection();

    EXPECT_TRUE(signalObserver.signalHasFired()) << "Selection Changed Signal should have fired";
    EXPECT_EQ(GlobalTextureToolSelectionSystem().countSelectedComponentNodes(), 0) << "Component selection should be gone now";
    EXPECT_EQ(GlobalTextureToolSelectionSystem().getMode(), textool::SelectionMode::Vertex) << "Should still be in vertex mode";
}

inline std::vector<Vector2> getTexcoords(const IFace* face)
{
    std::vector<Vector2> uvs;

    for (const auto& vertex : face->getWinding())
    {
        uvs.push_back(vertex.texcoord);
    }

    return uvs;
}

void dragManipulateSelectionTowardsLowerRight(const Vector2& startTexcoord, const render::View& view, bool cancelInsteadOfFinish = false)
{
    auto centroid = startTexcoord;
    auto centroidTransformed = view.GetViewProjection().transformPoint(Vector3(centroid.x(), centroid.y(), 0));
    Vector2 devicePoint(centroidTransformed.x(), centroidTransformed.y());

    GlobalTextureToolSelectionSystem().onManipulationStart();

    // Simulate a transformation by click-and-drag
    auto manipulator = GlobalTextureToolSelectionSystem().getActiveManipulator();
    EXPECT_EQ(manipulator->getType(), selection::IManipulator::Drag) << "Wrong manipulator";

    render::View scissored(view);
    ConstructSelectionTest(scissored, selection::Rectangle::ConstructFromPoint(devicePoint, Vector2(0.05, 0.05)));

    auto manipComponent = manipulator->getActiveComponent();
    auto pivot2World = GlobalTextureToolSelectionSystem().getPivot2World();
    manipComponent->beginTransformation(pivot2World, scissored, devicePoint);

    // Move the device point a bit to the lower right
    auto secondDevicePoint = devicePoint + (Vector2(1, -1) - devicePoint) / 2;

    render::View scissored2(view);
    ConstructSelectionTest(scissored2, selection::Rectangle::ConstructFromPoint(secondDevicePoint, Vector2(0.05, 0.05)));

    manipComponent->transform(pivot2World, scissored2, secondDevicePoint, selection::IManipulator::Component::Constraint::Unconstrained);

    if (!cancelInsteadOfFinish)
    {
        GlobalTextureToolSelectionSystem().onManipulationFinished();
    }
    else
    {
        GlobalTextureToolSelectionSystem().onManipulationCancelled();
    }
}

TEST_F(TextureToolTest, DragManipulateFace)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");

    // Put all faces into the tex tool scene
    Node_setSelected(brush, true);

    auto faceUp = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush), Vector3(0, 0, 1));
    auto faceDown = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush), Vector3(0, 0, -1));

    // Remember the texcoords of this face
    auto oldFaceUpUvs = getTexcoords(faceUp);
    auto oldFaceDownUvs = getTexcoords(faceDown);

    // Select the face
    auto textoolFace = findTexToolFaceWithNormal(faceUp->getPlane3().normal());
    textoolFace->setSelected(true);

    // Get the texture space bounds of this face
    // Construct a view that includes the patch UV bounds
    auto bounds = getTextureSpaceBounds(*faceUp);
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Check the device coords of the face centroid and manipulate from that point
    auto centroid = algorithm::getFaceCentroid(faceUp);
    dragManipulateSelectionTowardsLowerRight(centroid, view);

    // All the texcoords should have been moved to the lower right (U increased, V increased)
    auto oldUv = oldFaceUpUvs.begin();
    for (const auto& vertex : faceUp->getWinding())
    {
        EXPECT_LT(oldUv->x(), vertex.texcoord.x());
        EXPECT_LT(oldUv->y(), vertex.texcoord.y());
        ++oldUv;
    }

    // The texcoords of the other face should not have been changed
    oldUv = oldFaceDownUvs.begin();
    for (const auto& vertex : faceDown->getWinding())
    {
        EXPECT_EQ(oldUv->x(), vertex.texcoord.x());
        EXPECT_EQ(oldUv->y(), vertex.texcoord.y());
        ++oldUv;
    }
}

void performPatchManipulationTest(bool cancelOperation)
{
    auto patchNode = setupPatchNodeForTextureTool();
    auto patch = Node_getIPatch(patchNode);

    // Remember the texcoords before manipulation
    std::vector<Vector2> oldTexcoords;
    foreachPatchVertex(*patch, [&](const PatchControl& control) { oldTexcoords.push_back(control.texcoord); });

    auto texToolPatch = getFirstTextureToolNode();
    texToolPatch->setSelected(true);

    // Get the texture space bounds of this patch
    auto bounds = getTextureSpaceBounds(*patch);
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Check the device coords of a point in the middle of the patch
    auto centroid = Vector2(bounds.origin.x(), bounds.origin.y());
    dragManipulateSelectionTowardsLowerRight(centroid, view, cancelOperation); // optionally cancel

    std::vector<Vector2> changedTexcoords;
    foreachPatchVertex(*patch, [&](const PatchControl& control) { changedTexcoords.push_back(control.texcoord); });

    if (!cancelOperation)
    {
        // All the texcoords should have been moved to the lower right (U increased, V increased)
        for (auto i = 0; i < oldTexcoords.size(); ++i)
        {
            EXPECT_LT(oldTexcoords[i].x(), changedTexcoords[i].x());
            EXPECT_LT(oldTexcoords[i].y(), changedTexcoords[i].y());
        }
    }
    else
    {
        // All texcoords should remain unchanged
        for (auto i = 0; i < oldTexcoords.size(); ++i)
        {
            // should be unchanged
            EXPECT_NEAR(oldTexcoords[i].x(), changedTexcoords[i].x(), 0.01);
            EXPECT_NEAR(oldTexcoords[i].y(), changedTexcoords[i].y(), 0.01);
        }
    }
}

TEST_F(TextureToolTest, DragManipulatePatch)
{
    performPatchManipulationTest(false); // don't cancel
}

TEST_F(TextureToolTest, CancelDragManipulationOfPatch)
{
    performPatchManipulationTest(true); // cancel
}

void performPatchVertexManipulationTest(bool cancelOperation)
{
    auto patchNode = setupPatchNodeForTextureTool();
    auto patch = Node_getIPatch(patchNode);

    // Remember the texcoords before manipulation
    std::vector<Vector2> oldTexcoords;
    foreachPatchVertex(*patch, [&](const PatchControl& control) { oldTexcoords.push_back(control.texcoord); });

    // Get the texture space bounds of this patch
    auto bounds = getTextureSpaceBounds(*patch);
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    // Select every odd vertex
    for (auto i = 1; i < oldTexcoords.size(); i += 2)
    {
        performPointSelection(oldTexcoords[i], view);
    }

    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "No component node selected";

    // Drag-manipulate the first odd vertex
    dragManipulateSelectionTowardsLowerRight(oldTexcoords[1], view, cancelOperation); // optionally cancel the operation

    std::vector<Vector2> changedTexcoords;
    foreachPatchVertex(*patch, [&](const PatchControl& control) { changedTexcoords.push_back(control.texcoord); });

    if (!cancelOperation)
    {
        // All odd texcoords should have been moved to the lower right (U increased, V increased)
        for (auto i = 0; i < oldTexcoords.size(); ++i)
        {
            if (i % 2 == 1)
            {
                EXPECT_LT(oldTexcoords[i].x(), changedTexcoords[i].x());
                EXPECT_LT(oldTexcoords[i].y(), changedTexcoords[i].y());
            }
            else
            {
                // should be unchanged
                EXPECT_NEAR(oldTexcoords[i].x(), changedTexcoords[i].x(), 0.01);
                EXPECT_NEAR(oldTexcoords[i].y(), changedTexcoords[i].y(), 0.01);
            }
        }
    }
    else // operation cancelled
    {
        // All texcoords should remain unchanged
        for (auto i = 0; i < oldTexcoords.size(); ++i)
        {
            // should be unchanged
            EXPECT_NEAR(oldTexcoords[i].x(), changedTexcoords[i].x(), 0.01);
            EXPECT_NEAR(oldTexcoords[i].y(), changedTexcoords[i].y(), 0.01);
        }
    }
}

TEST_F(TextureToolTest, DragManipulatePatchVertices)
{
    performPatchVertexManipulationTest(false); // don't cancel
}

TEST_F(TextureToolTest, CancelDragManipulationOfPatchVertices)
{
    performPatchVertexManipulationTest(true); // cancel
}

// When switching from Vertex to Surface mode, the pivot should be recalculated
TEST_F(TextureToolTest, PivotIsRecalculatedWhenSwitchingModes)
{
    auto patchNode = setupPatchNodeForTextureTool();
    auto patch = Node_getIPatch(patchNode);

    // Get the texture space bounds of this patch
    auto bounds = getTextureSpaceBounds(*patch);

    // Construct a view that includes the patch UV bounds
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    // Get the texcoords of the first vertex
    auto firstVertex = patch->ctrlAt(2, 1).texcoord;
    auto secondVertex = patch->ctrlAt(2, 0).texcoord;

    // Select the patch itself
    performPointSelection(secondVertex, view);

    // Check the manipulation pivot
    auto pivot2world = GlobalTextureToolSelectionSystem().getPivot2World();

    // The pivot should be near the center of the patch
    auto boundsOrigin = getTextureSpaceBounds(*patch).origin;
    EXPECT_TRUE(math::isNear(pivot2world.tCol().getVector3(), boundsOrigin, 0.01)) <<
        "Pivot should be at the center of the patch";

    // Switch to vertex selection mode and select two vertices
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    performPointSelection(firstVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one patch should still be selected";

    // Pivot should be right at the first vertex
    auto componentPivot2world = GlobalTextureToolSelectionSystem().getPivot2World();
    EXPECT_TRUE(math::isNear(componentPivot2world.tCol().getVector3(),
        Vector3(firstVertex.x(), firstVertex.y(), 0), 0.01)) << "Pivot should be at the single selected vertex";

    // Selecting a second point, the pivot should move to the middle of the two
    performPointSelection(secondVertex, view);
    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "Only one patch should still be selected";

    componentPivot2world = GlobalTextureToolSelectionSystem().getPivot2World();

    // The pivot should now be in the middle of the two selected vertices
    auto midPoint = (firstVertex + secondVertex) * 0.5;
    EXPECT_TRUE(math::isNear(componentPivot2world.tCol().getVector3(), 
        Vector3(midPoint.x(), midPoint.y(), 0), 0.01)) << "Pivot should be in between the two selected vertices";

    // Switching back to surface selection mode, the pivot needs to move to the bounds origin again
    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Surface);
    EXPECT_TRUE(math::isNear(pivot2world.tCol().getVector3(), boundsOrigin, 0.01)) <<
        "Pivot should be at the center of the patch after switching back to surface mode";
}

void performFaceVertexManipulationTest(bool cancelOperation, std::vector<std::size_t> vertexIndicesToManipulate,
    std::function<void(IFace&, const std::vector<Vector2>&, const std::vector<Vector2>&)> assertionFunc)
{
    auto worldspawn = GlobalMapModule().findOrInsertWorldspawn();
    auto brush = algorithm::createCubicBrush(worldspawn, Vector3(0, 256, 256), "textures/numbers/1");
    scene::addNodeToContainer(brush, worldspawn);

    // Put all faces into the tex tool scene
    Node_setSelected(brush, true);

    auto faceUp = algorithm::findBrushFaceWithNormal(Node_getIBrush(brush), Vector3(0, 0, 1));

    // Remember the texcoords before manipulation
    std::vector<Vector2> oldTexcoords;
    for (const auto& vertex : faceUp->getWinding()) { oldTexcoords.push_back(vertex.texcoord); }

    // Get the texture space bounds of this face
    auto bounds = getTextureSpaceBounds(*faceUp);
    bounds.extents *= 1.2f;

    render::TextureToolView view;
    view.constructFromTextureSpaceBounds(bounds, TEXTOOL_WIDTH, TEXTOOL_HEIGHT);

    GlobalTextureToolSelectionSystem().setMode(textool::SelectionMode::Vertex);

    // Select a certain number of vertices
    for (auto index : vertexIndicesToManipulate)
    {
        performPointSelection(oldTexcoords[index], view);
    }

    EXPECT_EQ(getAllSelectedComponentNodes().size(), 1) << "No component node selected";

    // Drag-manipulate the first odd vertex
    auto firstIndex = vertexIndicesToManipulate.front();
    dragManipulateSelectionTowardsLowerRight(oldTexcoords[firstIndex], view, cancelOperation); // optionally cancel the operation

    std::vector<Vector2> changedTexcoords;
    for (const auto& vertex : faceUp->getWinding()) { changedTexcoords.push_back(vertex.texcoord); }

    assertionFunc(*faceUp, oldTexcoords, changedTexcoords);
}

inline void assertAllCoordsUnchanged(IFace&, const std::vector<Vector2>& oldTexcoords, const std::vector<Vector2>& changedTexcoords)
{
    // All texcoords should remain unchanged
    for (auto i = 0; i < oldTexcoords.size(); ++i)
    {
        // should be unchanged
        EXPECT_NEAR(oldTexcoords[i].x(), changedTexcoords[i].x(), 0.01);
        EXPECT_NEAR(oldTexcoords[i].y(), changedTexcoords[i].y(), 0.01);
    }
}

inline void assertAllCoordsMovedBySameAmount(IFace&, const std::vector<Vector2>& oldTexcoords, const std::vector<Vector2>& changedTexcoords)
{
    // All manipulated vertices should have been changed by the same amount
    auto draggedDistanceOfFirst = (changedTexcoords[0] - oldTexcoords[0]).getLengthSquared();

    EXPECT_GT(draggedDistanceOfFirst, 0) << "Vertex 0 hasn't been moved at all";

    for (int i = 1; i < changedTexcoords.size(); ++i)
    {
        auto draggedDistance = (changedTexcoords[i] - oldTexcoords[i]).getLengthSquared();
        EXPECT_NEAR(draggedDistance, draggedDistanceOfFirst, 0.01)
            << "The vertex " << i << " should have been moved by the same amount as vertex 0";
    }
}

inline std::size_t getFarthestIndex(const std::vector<Vector2>& texcoords, const Vector2& point, std::vector<std::size_t> fixedIndices)
{
    std::size_t farthestIndex = 1;
    double largestDistance = 0;

    for (std::size_t i = 0; i < texcoords.size(); ++i)
    {
        if (std::find(fixedIndices.begin(), fixedIndices.end(), i) != fixedIndices.end()) continue;

        auto candidateDistance = (texcoords[i] - point).getLengthSquared();

        if (candidateDistance > largestDistance)
        {
            farthestIndex = i;
            largestDistance = candidateDistance;
        }
    }

    return farthestIndex;
}

// When manipulating one vertex, the "opposite" vertex should remain the same as it is chosen as fixed point
TEST_F(TextureToolTest, DragManipulateSingleFaceVertex)
{
    performFaceVertexManipulationTest(false, { 0 }, [](IFace& face, 
        const std::vector<Vector2>& oldTexcoords, const std::vector<Vector2>& changedTexcoords)
    {
        // Find out which face vertex was the farthest away from the modified one
        auto farthestIndex = getFarthestIndex(oldTexcoords, oldTexcoords[0], { 0 });

        // The farthest vertex should remain unchanged
        EXPECT_NEAR(oldTexcoords[farthestIndex].x(), changedTexcoords[farthestIndex].x(), 0.01) << "Opposite vertex X should remain unchanged";
        EXPECT_NEAR(oldTexcoords[farthestIndex].y(), changedTexcoords[farthestIndex].y(), 0.01) << "Opposite vertex Y should remain unchanged";

        // The algorithm will pick a third vertex that should remain unchanged
        // it will be the farthest from the center of the first two vertices
        auto center = (oldTexcoords[farthestIndex] + oldTexcoords[0]) * 0.5;
        auto thirdIndex = getFarthestIndex(oldTexcoords, center, { 0, farthestIndex });

        // All the others will have changed in some way
        for (int i = 0; i < oldTexcoords.size(); ++i)
        {
            if (i == farthestIndex || i == thirdIndex) continue;

            EXPECT_FALSE(float_equal_epsilon(oldTexcoords[i].x(), changedTexcoords[i].x(), 0.05)) << "Vertex " << i << " x should have changed";
            EXPECT_FALSE(float_equal_epsilon(oldTexcoords[i].y(), changedTexcoords[i].y(), 0.05)) << "Vertex " << i << " y should have changed";;
        }
    }); 
}

// Dragging two selected vertices chooses the one vertex as anchor point which is farthest away from the clicked vertex
TEST_F(TextureToolTest, DragManipulateTwoFaceVertices)
{
    // Vertices 0 and 2 are opposite of each other
    std::size_t firstVertex = 0;
    std::size_t secondVertex = 2;
    performFaceVertexManipulationTest(false, { firstVertex, secondVertex }, [&](IFace& face,
        const std::vector<Vector2>& oldTexcoords, const std::vector<Vector2>& changedTexcoords)
    {
        auto center = (changedTexcoords[firstVertex] + changedTexcoords[secondVertex]) * 0.5;
        // Find out which face vertex was the farthest away from the bounds center of the selection
        std::size_t farthestIndex = 0;
        double largestDistanceSquared = 0;

        for (std::size_t i = 0; i < changedTexcoords.size(); ++i)
        {
            if (i == firstVertex || i == secondVertex) continue;

            auto candidateDistanceSquared = (changedTexcoords[i] - center).getLengthSquared();

            if (candidateDistanceSquared > largestDistanceSquared)
            {
                farthestIndex = i;
                largestDistanceSquared = candidateDistanceSquared;
            }
        }

        // The farthest vertex should remain unchanged
        EXPECT_NEAR(oldTexcoords[farthestIndex].x(), changedTexcoords[farthestIndex].x(), 0.01) << "Opposite vertex X should remain unchanged";
        EXPECT_NEAR(oldTexcoords[farthestIndex].y(), changedTexcoords[farthestIndex].y(), 0.01) << "Opposite vertex Y should remain unchanged";

        // The two manipulated vertices should have been changed by the same amount
        auto draggedDistanceOfFirst = changedTexcoords[firstVertex] - oldTexcoords[firstVertex];
        auto draggedDistanceOfSecond = changedTexcoords[secondVertex] - oldTexcoords[secondVertex];

        EXPECT_NEAR(draggedDistanceOfFirst.getLengthSquared(), draggedDistanceOfSecond.getLengthSquared(), 0.01);

        // All the other non-fixed vertices will have changed in some way
        for (int i = 0; i < oldTexcoords.size(); ++i)
        {
            if (i == firstVertex || i == secondVertex || i == farthestIndex) continue;

            EXPECT_FALSE(float_equal_epsilon(oldTexcoords[i].x(), changedTexcoords[i].x(), 0.05)) << "Vertex " << i << " x should have changed";
            EXPECT_FALSE(float_equal_epsilon(oldTexcoords[i].y(), changedTexcoords[i].y(), 0.05)) << "Vertex " << i << " y should have changed";;
        }
    });
}

// Dragging three (or more) selected vertices should move all of the face vertices by the same amount
TEST_F(TextureToolTest, DragManipulateThreeFaceVertices)
{
    // Select three vertices
    performFaceVertexManipulationTest(false, { 0, 1, 2 }, assertAllCoordsMovedBySameAmount);
}

// Dragging three (or more) selected vertices should move all of the face vertices by the same amount
TEST_F(TextureToolTest, DragManipulateFourFaceVertices)
{
    // Select four vertices
    performFaceVertexManipulationTest(false, { 0, 1, 2, 3 }, assertAllCoordsMovedBySameAmount);
}

TEST_F(TextureToolTest, CancelDragManipulationOfFaceVertices)
{
    performFaceVertexManipulationTest(true, { 0 }, assertAllCoordsUnchanged); // cancel
}

}
