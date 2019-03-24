#pragma once

class IGraphicsContext
{
	virtual void Initialise() = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void Resize() = 0;
};