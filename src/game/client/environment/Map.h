//
// Created by danii on 09.09.2023.
//

#ifndef DDNET_MAP_H
#define DDNET_MAP_H

#include "engine/client.h"
#include "game/client/component.h"

using namespace std;

#define MAP_TILE_AIR 0
#define MAP_TILE_HOOKABLE_GROUND 1
#define MAP_TILE_NON_HOOKABLE_GROUND 3

struct MapGraphNode {
	ivec2 connectionWith;
	int cost;

	MapGraphNode()
	{
	}

	MapGraphNode(ivec2 connectionWith, int cost)
	{
		this->connectionWith = connectionWith;
		this->cost = cost;
	}

	MapGraphNode(int x, int y, int cost)
	{
		this->connectionWith = ivec2(x, y);
		this->cost = cost;
	}
};
typedef vector<MapGraphNode> MapGraphCell;
typedef vector<MapGraphCell> MapGraphLine;
typedef vector<MapGraphLine> MapGraph;

struct Node {
	int x, y;
	int cost;
	Node* parent;

	bool operator==(const Node& other) const {
		return x == other.x && y == other.y;
	}
};

struct PriorityQueueCompare {
	bool operator()(const Node* a, const Node* b) {
		return a->cost > b->cost;
	}
};

class Map : public CComponent
{
public:
	void scanTiles();

	void buildGraph();
	vector<Node*> aStar(ivec2 startPosition, ivec2 endPosition);
	int findGroundNear(ivec2 position);
	int findGroundBottom(ivec2 position);

	int getTile(int mapX, int mapY);
	int getTile(ivec2 mapPosition);

	bool canWalk(ivec2 startPosition, ivec2 endPosition);
	bool canFallFromGround(ivec2 startPosition, ivec2 endPosition);
	bool canFall(ivec2 startPosition, ivec2 endPosition);
	bool canJumpOnWall(ivec2 startPosition, ivec2 endPosition);

	bool isTileGround(int tileId);

	virtual void OnMapLoad() override;
	virtual int Sizeof() const override { return sizeof(*this); }

	vec2 convertWorldToUI(vec2 worldPos);
	vec2 convertMapToUI(vec2 mapPos);
protected:
	vector<vector<int> > tiles;
	MapGraph graph;
};

#endif // DDNET_MAP_H
