//
// Created by danii on 09.09.2023.
//

#ifndef DDNET_MOVEMENTAGENT_H
#define DDNET_MOVEMENTAGENT_H


#include "engine/client.h"
#include "game/client/component.h"
#include "game/client/environment/Map.h"

class MovementAgent : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }

	void moveTo(vec2 position);
	void moveToOnGround(vec2 position);

	bool isTwoPointsOnLinearGround(vec2 firstPoint, vec2 secondPoint);

	void OnUpdate();
	void OnRender();

protected:
	vec2 moveToPosition;
	bool isMoving = false;
	vector<Node*> path;
	int pathNumber = 0;
};

#endif // DDNET_MOVEMENTAGENT_H
