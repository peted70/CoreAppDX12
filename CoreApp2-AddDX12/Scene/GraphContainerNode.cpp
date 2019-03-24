#include "pch.h"
#include "GraphContainerNode.h"

GraphContainerNode::GraphContainerNode(int index) : 
	_index(index)
{
	CoCreateGuid(&_guid);
}

GraphContainerNode::~GraphContainerNode()
{
}

void GraphContainerNode::Update(StepTimer const& timer)
{
	for (auto child : _children)
	{
		child->Update(timer);
	}
}

XMMATRIX GraphContainerNode::PreDraw(IGraphicsContext& context, XMMATRIX model)
{
	// Don't execute this until loaded..

	XMMATRIX mat;
	if (_hasMatrix)
	{
		mat = XMLoadFloat4x4(&_matrix);
	}
	else
	{
		mat = 
			XMMatrixTranspose(
				XMMatrixAffineTransformation(
					XMLoadFloat3(&_scale), 
					XMLoadFloat3(&emptyVector), 
					XMLoadFloat4(&_rotation), 
					XMLoadFloat3(&_translation)));
	}
	if (!XMMatrixIsIdentity(model))
	{
		mat = XMMatrixMultiply(model, mat);
	}
	model = mat;

	// Prepare to pass the updated model matrix to the shader 
	//XMStoreFloat4x4(&BufferManager::Instance().MVPBuffer().BufferData().model, mat);
	//BufferManager::Instance().MVPBuffer().Update(*(DevResources()));
	return mat;
}

void GraphContainerNode::Draw(IGraphicsContext& context, XMMATRIX model)
{
	for (auto child : _children)
	{
		auto modelMatrix = child->PreDraw(context, model);
		child->Draw(context, modelMatrix);
	}
}

GraphNode *GraphContainerNode::FindChildByIndex(int index)
{
	if (Index() == index)
		return this;
	for (auto child : _children)
	{
		auto ret = child->FindChildByIndex(index);
		if (ret != nullptr)
			return ret;
	}
	return nullptr;
}

GraphNode *GraphContainerNode::FindChildById(GUID id)
{
	if (_guid == id)
		return this;
	for (auto child : _children)
	{
		return child->FindChildById(id);
	}
	return nullptr;
}

void GraphContainerNode::ForAllChildrenRecursive(function<void(GraphNode&)> func)
{
	//func(*this);
	for (auto child : _children)
	{
		func(*child);
		child->ForAllChildrenRecursive(func);
	}
}

void GraphContainerNode::ForAllChildrenRecursiveUntil(function<bool(GraphNode&)> func)
{
	auto ret = func(*this);
	if (!ret)
		return;

	for (auto child : _children)
	{
		ret = func(*child);
		if (!ret)
			return;
		child->ForAllChildrenRecursive(func);
	}
}

BoundingBox<float> GraphContainerNode::GetBoundingBox()
{
	BoundingBox<float> ret;
	for (auto child : _children)
	{
		ret.Grow(child->GetBoundingBox());
	}
	return ret;
}

void GraphContainerNode::CreateDeviceDependentResources()
{
	for (auto child : _children)
	{
		child->CreateDeviceDependentResources();
	}
}

void GraphContainerNode::Initialise()
{
}

void GraphContainerNode::AddChild(shared_ptr<GraphNode> child)
{
	_children.push_back(child);
}

size_t GraphContainerNode::NumChildren()
{
	return _children.size();
}

shared_ptr<GraphNode> GraphContainerNode::GetChild(int i)
{
	return _children[i];
}

const wstring& GraphContainerNode::Name() const
{
	return _name;
}

void GraphContainerNode::SetName(const wstring& name)
{
	if (_name != name)
		_name = name;
}

bool GraphContainerNode::IsSelected()
{
	return _selected;
}

void GraphContainerNode::SetSelected(bool sel)
{
	if (sel == _selected)
		return;

	// Set all child nodes accordingly..
	ForAllChildrenRecursive([sel](GraphNode& node)
	{
		node.SetSelected(sel);
	});
}

void GraphContainerNode::CreateTransform()
{
}
