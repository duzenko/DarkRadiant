#include "RotateManipulator.h"

#include "igl.h"
#include "selection/SelectionPool.h"
#include "selection/BestPoint.h"
#include "selection/TransformationVisitors.h"
#include <fmt/format.h>

namespace selection
{

template<typename remap_policy>
inline void draw_semicircle(const std::size_t segments, const float radius, VertexCb* vertices, remap_policy remap) {
  const double increment = math::PI / double(segments << 2);

  std::size_t count = 0;
  float x = radius;
  float y = 0;
  remap_policy::set(vertices[segments << 2].vertex, -radius, 0, 0);
  while(count < segments)
  {
    VertexCb* i = vertices + count;
    VertexCb* j = vertices + ((segments << 1) - (count + 1));

    VertexCb* k = i + (segments << 1);
    VertexCb* l = j + (segments << 1);

#if 0
    VertexCb* m = i + (segments << 2);
    VertexCb* n = j + (segments << 2);
    VertexCb* o = k + (segments << 2);
    VertexCb* p = l + (segments << 2);
#endif

    remap_policy::set(i->vertex, x,-y, 0);
    remap_policy::set(k->vertex,-y,-x, 0);
#if 0
    remap_policy::set(m->vertex,-x, y, 0);
    remap_policy::set(o->vertex, y, x, 0);
#endif

    ++count;

    {
      const double theta = increment * count;
      x = static_cast<float>(radius * cos(theta));
      y = static_cast<float>(radius * sin(theta));
    }

    remap_policy::set(j->vertex, y,-x, 0);
    remap_policy::set(l->vertex,-x,-y, 0);
#if 0
    remap_policy::set(n->vertex,-y, x, 0);
    remap_policy::set(p->vertex, x, y, 0);
#endif
  }
}

// Constructor
RotateManipulator::RotateManipulator(ManipulationPivot& pivot, std::size_t segments, float radius) :
	_pivot(pivot),
	_pivotTranslatable(_pivot),
    _rotateFree(*this),
    _rotateAxis(*this),
	_translatePivot(_pivotTranslatable),
    _circleX((segments << 2) + 1),
    _circleY((segments << 2) + 1),
    _circleZ((segments << 2) + 1),
    _circleScreen(segments<<3),
    _circleSphere(segments<<3),
	_pivotPoint(GL_POINTS)
{
	draw_semicircle(segments, radius, &_circleX.front(), RemapYZX());
    draw_semicircle(segments, radius, &_circleY.front(), RemapZXY());
    draw_semicircle(segments, radius, &_circleZ.front(), RemapXYZ());

	draw_circle(segments, radius * 1.15f, &_circleScreen.front(), RemapXYZ());
    draw_circle(segments, radius, &_circleSphere.front(), RemapXYZ());

	_pivotPoint.push_back(VertexCb(Vertex3f(0,0,0), ManipulatorBase::COLOUR_SPHERE()));
}

void RotateManipulator::UpdateColours()
{
    _circleX.setColour(colourSelected(COLOUR_X(), _selectableX.isSelected()));
	_circleY.setColour(colourSelected(COLOUR_Y(), _selectableY.isSelected()));
	_circleZ.setColour(colourSelected(COLOUR_Z(), _selectableZ.isSelected()));
    _circleScreen.setColour(colourSelected(ManipulatorBase::COLOUR_SCREEN(), _selectableScreen.isSelected()));
    _circleSphere.setColour(colourSelected(ManipulatorBase::COLOUR_SPHERE(), false));
	_pivotPoint.setColour(colourSelected(ManipulatorBase::COLOUR_SPHERE(), _selectablePivotPoint.isSelected()));
}

void RotateManipulator::updateCircleTransforms()
{
    Vector3 localViewpoint(
		_pivot2World._worldSpace.getTransposed().transformDirection(_pivot2World._viewpointSpace.zCol().getVector3())
    );

    _circleX_visible = !math::isNear(g_vector3_axis_x, localViewpoint, 1e-6);
    if(_circleX_visible)
    {
      _local2worldX = Matrix4::getIdentity();
      _local2worldX.yCol().getVector3() = g_vector3_axis_x.cross(localViewpoint).getNormalised();
      _local2worldX.zCol().getVector3() = _local2worldX.xCol().getVector3().cross(
        											_local2worldX.yCol().getVector3()).getNormalised();
	  _local2worldX.premultiplyBy(_pivot2World._worldSpace);
    }

	_circleY_visible = !math::isNear(g_vector3_axis_y, localViewpoint, 1e-6);
    if(_circleY_visible)
    {
      _local2worldY = Matrix4::getIdentity();
      _local2worldY.zCol().getVector3() = g_vector3_axis_y.cross(localViewpoint).getNormalised();
      _local2worldY.xCol().getVector3() = _local2worldY.yCol().getVector3().cross(
      													_local2worldY.zCol().getVector3()).getNormalised();
      _local2worldY.premultiplyBy(_pivot2World._worldSpace);
    }

	_circleZ_visible = !math::isNear(g_vector3_axis_z, localViewpoint, 1e-6);
    if(_circleZ_visible)
    {
      _local2worldZ = Matrix4::getIdentity();
      _local2worldZ.xCol().getVector3() = g_vector3_axis_z.cross(localViewpoint).getNormalised();
      _local2worldZ.yCol().getVector3() = _local2worldZ.zCol().getVector3().cross(
      												_local2worldZ.xCol().getVector3()).getNormalised();
	  _local2worldZ.premultiplyBy(_pivot2World._worldSpace);
    }
}

void RotateManipulator::render(RenderableCollector& collector, const VolumeTest& volume)
{
    _pivot2World.update(_pivot.getMatrix4(), volume.GetModelview(), volume.GetProjection(), volume.GetViewport());
    updateCircleTransforms();

    // temp hack
    UpdateColours();

    collector.addRenderable(*_stateOuter, _circleScreen, _pivot2World._viewpointSpace);
    collector.addRenderable(*_stateOuter, _circleSphere, _pivot2World._viewpointSpace);

    if(_circleX_visible)
    {
      collector.addRenderable(*_stateOuter, _circleX, _local2worldX);
    }
    if(_circleY_visible)
    {
      collector.addRenderable(*_stateOuter, _circleY, _local2worldY);
    }
    if(_circleZ_visible)
    {
      collector.addRenderable(*_stateOuter, _circleZ, _local2worldZ);
    }

	collector.addRenderable(*_pivotPointShader, _pivotPoint, _pivot2World._worldSpace);

	collector.addRenderable(*_pivotPointShader, *this, Matrix4::getIdentity());
}

std::string RotateManipulator::getRotationAxisName() const
{
    if (_selectableX.isSelected()) return "X";
    if (_selectableY.isSelected()) return "Y";
    if (_selectableZ.isSelected()) return "Z";
    
    return std::string();
}

void RotateManipulator::render(const RenderInfo& info) const
{
    if (_selectableX.isSelected() || _selectableY.isSelected() ||
        _selectableZ.isSelected() || _selectableScreen.isSelected())
	{
		glColor3d(0.75, 0, 0);

		glRasterPos3dv(_pivot2World._worldSpace.tCol().getVector3() - Vector3(10, 10, 10));

		double angle = static_cast<double>(c_RAD2DEGMULT * _rotateAxis.getCurAngle());
        auto rotationAxisName = getRotationAxisName();

        _glFont->drawString(fmt::format("Rotate: {0:3.2f} degrees {1}", angle, rotationAxisName));
	}
}

void RotateManipulator::testSelect(SelectionTest& test, const Matrix4& pivot2world)
{
    _pivot2World.update(_pivot.getMatrix4(), test.getVolume().GetModelview(),
        test.getVolume().GetProjection(), test.getVolume().GetViewport());
    updateCircleTransforms();

    SelectionPool selector;

	if (test.getVolume().TestPoint(_pivot.getVector3()))
	{
		selector.addSelectable(SelectionIntersection(0, 0), &_selectablePivotPoint);
	}
	else
	{
		{
			{
				Matrix4 local2view(test.getVolume().GetViewProjection().getMultipliedBy(_local2worldX));

				SelectionIntersection best;
				LineStrip_BestPoint(local2view, &_circleX.front(), _circleX.size(), best);
				selector.addSelectable(best, &_selectableX);
			}

			{
				Matrix4 local2view(test.getVolume().GetViewProjection().getMultipliedBy(_local2worldY));

				SelectionIntersection best;
				LineStrip_BestPoint(local2view, &_circleY.front(), _circleY.size(), best);
				selector.addSelectable(best, &_selectableY);
			}

			{
				Matrix4 local2view(test.getVolume().GetViewProjection().getMultipliedBy(_local2worldZ));

				SelectionIntersection best;
				LineStrip_BestPoint(local2view, &_circleZ.front(), _circleZ.size(), best);
				selector.addSelectable(best, &_selectableZ);
			}
		}

		{
			Matrix4 local2view(test.getVolume().GetViewProjection().getMultipliedBy(_pivot2World._viewpointSpace));

			{
				SelectionIntersection best;
				LineLoop_BestPoint(local2view, &_circleScreen.front(), _circleScreen.size(), best);
				selector.addSelectable(best, &_selectableScreen);
			}

			{
				SelectionIntersection best;
				Circle_BestPoint(local2view, eClipCullCW, &_circleSphere.front(), _circleSphere.size(), best);
				selector.addSelectable(best, &_selectableSphere);
			}
		}
	}

    _axisScreen = _pivot2World._axisScreen;

    if (!selector.empty())
    {
		selector.begin()->second->setSelected(true);
    }
}

RotateManipulator::Component* RotateManipulator::getActiveComponent()
{
	if (_selectablePivotPoint.isSelected())
	{
		return &_translatePivot;
	}

    if(_selectableX.isSelected()) {
      _rotateAxis.SetAxis(g_vector3_axis_x);
      return &_rotateAxis;
    }
    else if(_selectableY.isSelected()) {
      _rotateAxis.SetAxis(g_vector3_axis_y);
      return &_rotateAxis;
    }
    else if(_selectableZ.isSelected()) {
      _rotateAxis.SetAxis(g_vector3_axis_z);
      return &_rotateAxis;
    }
    else if(_selectableScreen.isSelected()) {
      _rotateAxis.SetAxis(_axisScreen);
      return &_rotateAxis;
    }
    else
      return &_rotateFree;
}

void RotateManipulator::setSelected(bool select)
{
    _selectableX.setSelected(select);
    _selectableY.setSelected(select);
    _selectableZ.setSelected(select);
    _selectableScreen.setSelected(select);
	_selectablePivotPoint.setSelected(select);

	if (!select)
	{
		_rotateAxis.resetCurAngle();
	}
}

bool RotateManipulator::isSelected() const
{
    return _selectableX.isSelected() ||
		_selectableY.isSelected() ||
		_selectableZ.isSelected() ||
		_selectableScreen.isSelected() ||
		_selectableSphere.isSelected() ||
		_selectablePivotPoint.isSelected();
}

void RotateManipulator::rotate(const Quaternion& rotation)
{
	// Perform the rotation according to the current mode
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent)
	{
		GlobalSelectionSystem().foreachSelectedComponent(
			RotateComponentSelected(rotation, _pivot.getVector3()));
	}
	else
	{
		// Cycle through the selections and rotate them
		GlobalSelectionSystem().foreachSelected(RotateSelected(rotation, _pivot.getVector3()));
	}

	SceneChangeNotify();
}

// Static members
ShaderPtr RotateManipulator::_stateOuter;
ShaderPtr RotateManipulator::_pivotPointShader;
IGLFont::Ptr RotateManipulator::_glFont;

}
