#pragma once
#include "GraphNode.h"
#include <vector>
#include <string>
#include "../StepTimer.h"
#include <Rpc.h>
#include "../Graphics/DX12/DX12GraphicsContext.h"

using namespace std;

const XMFLOAT3 emptyVector = { 0, 0, 0 };

class GraphContainerNode :
	public GraphNode
{
public:
	GraphContainerNode(int index);
	virtual ~GraphContainerNode();

	virtual void Update(StepTimer const& timer);

	virtual XMMATRIX PreDraw(IGraphicsContext& context, XMMATRIX model);
	virtual void Draw(IGraphicsContext& context, XMMATRIX model);
	virtual void CreateDeviceDependentResources();
	virtual void Initialise();

	virtual void AfterLoad() override { m_loadingComplete = true; };
	virtual void ForAllChildrenRecursive(function<void(GraphNode&)> func) override;
	virtual void ForAllChildrenRecursiveUntil(function<bool(GraphNode&)> func) override;
	virtual GraphNode *FindChildByIndex(int index) override;
	virtual GraphNode *FindChildById(GUID id) override;

	virtual void AddChild(shared_ptr<GraphNode> child);
	virtual size_t NumChildren() override;
	virtual shared_ptr<GraphNode> GetChild(int i) override;
	virtual const wstring& Name() const override;
	virtual void SetName(const wstring& name)  override;

	virtual void CreateTransform();
	virtual int Index() override { return _index; };
	virtual GUID GetId() override { return _guid; }

	virtual bool IsSelected() override;
	virtual void SetSelected(bool sel) override ;

	virtual BoundingBox<float> GetBoundingBox() override;

	bool IsEqual(const GraphContainerNode& other)
	{
		return _guid == other._guid;
	}

	float GetRoll(XMFLOAT4 q)
	{
		float x = XMConvertToDegrees(atan2
		(
			2 * q.x * q.w - 2 * q.y * q.z,
			1 - 2 * pow(q.x, 2.0f) - 2 * pow(q.z, 2.0f)
		));

		if (q.x * q.y + q.z * q.w == 0.5)
		{
			x = XMConvertToDegrees((float)(2 * atan2(q.x, q.w)));
		}

		else if (q.x * q.y + q.z * q.w == -0.5)
		{
			x = XMConvertToDegrees((float)(-2 * atan2(q.x, q.w)));
		}
		return x;
	}

	float GetPitch(XMFLOAT4 q)
	{
		float y = XMConvertToDegrees(atan2
		(
			2 * q.y * q.w - 2 * q.x * q.z,
			1 - 2 * pow(q.y, 2.0f) - 2 * pow(q.z, 2.0f)
		));
		if (q.x * q.y + q.z * q.w == 0.5)
		{
			y = 0;
		}
		else if (q.x * q.y + q.z * q.w == -0.5)
		{
			y = 0;
		}
		return y;
	}

	float GetYaw(XMFLOAT4 q)
	{
		float z = XMConvertToDegrees(asin
		(
			2 * q.x * q.y + 2 * q.z * q.w
		));
		return z;
	}

	XMFLOAT3 QuaternionToEuler(XMFLOAT4 q)
	{
		XMFLOAT3 v = { 0.0f, 0.0f, 0.0f };

		v.y = XMConvertToDegrees(atan2
		(
			2 * q.y * q.w - 2 * q.x * q.z,
			1 - 2 * pow(q.y, 2.0f) - 2 * pow(q.z, 2.0f)
		));

		v.z = XMConvertToDegrees(asin
		(
			2 * q.x * q.y + 2 * q.z * q.w
		));

		v.x = XMConvertToDegrees(atan2
		(
			2 * q.x * q.w - 2 * q.y * q.z,
			1 - 2 * pow(q.x, 2.0f) - 2 * pow(q.z, 2.0f)
		));

		if (q.x * q.y + q.z * q.w == 0.5)
		{
			v.x = XMConvertToDegrees((float)(2 * atan2(q.x, q.w)));
			v.y = 0;
		}

		else if (q.x * q.y + q.z * q.w == -0.5)
		{
			v.x = XMConvertToDegrees((float)(-2 * atan2(q.x, q.w)));
			v.y = 0;
		}

		return v;
	}

	XMFLOAT3 GetScale() { return _scale; }
	XMFLOAT3 GetTranslation() { return _translation; }
	
	float GetRotationX()
	{
		return GetRoll(_rotation);
	}
	
	float GetRotationY()
	{
		return GetPitch(_rotation);
	}
	
	float GetRotationZ()
	{
		return GetYaw(_rotation);
	}

	XMFLOAT3 GetRotation()
	{
		return QuaternionToEuler(_rotation);
	}

	void SetTranslation(float x, float y, float z)
	{
		_translation.x = x;
		_translation.y = y;
		_translation.z = z;
	}

	void SetRotationYaw(float yaw)
	{
		XMVECTOR vec = { XMConvertToRadians(_roll), XMConvertToRadians(_pitch), XMConvertToRadians(yaw) };
		auto quat = XMQuaternionRotationRollPitchYawFromVector(vec);
		XMStoreFloat4(&_rotation, quat);
		_yaw = yaw;
	}

	void SetRotationPitch(float pitch)
	{
		XMVECTOR vec = { XMConvertToRadians(_roll), XMConvertToRadians(pitch), XMConvertToRadians(_yaw) };
		auto quat = XMQuaternionRotationRollPitchYawFromVector(vec);
		XMStoreFloat4(&_rotation, quat);
		_pitch = pitch;
	}

	void SetRotationRoll(float roll)
	{
		XMVECTOR vec = { XMConvertToRadians(roll), XMConvertToRadians(_pitch), XMConvertToRadians(_yaw) };
		auto quat = XMQuaternionRotationRollPitchYawFromVector(vec);
		XMStoreFloat4(&_rotation, quat);
		_roll = roll;
	}

	void SetScale(float x, float y, float z)
	{
		_scale.x = x;
		_scale.y = y;
		_scale.z = z;
	}

protected:
	XMFLOAT4X4 _matrix;
	bool _hasMatrix = false;

	XMFLOAT3 _scale = { 1.0f, 1.0f, 1.0f };
	XMFLOAT3 _translation = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4 _rotation = { 0.0f, 0.0f, 0.0f, 0.0f };
	float _yaw, _pitch, _roll;

	int _index;

	vector<shared_ptr<GraphNode>>_children;
	wstring _name;
	bool _selected = false;
	GUID _guid;
	bool m_loadingComplete = false;
};

