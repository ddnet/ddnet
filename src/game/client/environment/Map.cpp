//
// Created by danii on 09.09.2023.
//

#include "Map.h"
#include "game/client/gameclient.h"

int manhattan_distance(Node* a, Node* b) {
	return std::abs(a->x - b->x) + std::abs(a->y - b->y);
}

std::vector<Node*> construct_path(Node* end) {
	std::vector<Node*> path;
	Node* current = end;
	while (current) {
		path.push_back(current);
		current = current->parent;
	}
	std::reverse(path.begin(), path.end());
	return path;
}

void Map::scanTiles()
{
	this->tiles = vector<vector<int> >();

	for (int x = 0; x < Collision()->GetWidth(); x++)
	{
		vector<int> mapCol;

		for (int y = 0; y < Collision()->GetHeight(); y++)
		{
			mapCol.push_back(Collision()->GetTile(x * 32, y * 32));
		}

		this->tiles.push_back(mapCol);
	}
}

void Map::OnMapLoad()
{
	this->scanTiles();
	this->buildGraph();
}

int Map::getTile(int mapX, int mapY)
{
	if (mapX < 0 || mapX >= GameClient()->Collision()->GetWidth() || mapY < 0 || mapY >= GameClient()->Collision()->GetHeight()) {
		return MAP_TILE_UNDER_BORDER;
	}

	return this->tiles[mapX][mapY];
}

int Map::getTile(ivec2 mapPosition)
{
	return this->getTile(mapPosition.x, mapPosition.y);
}

bool Map::isTileGround(int tileId)
{
	return tileId == MAP_TILE_HOOKABLE_GROUND || tileId == MAP_TILE_NON_HOOKABLE_GROUND;
}

bool Map::canWalk(ivec2 startPosition, ivec2 endPosition)
{
	if (startPosition.y != endPosition.y) {
		return false;
	}

	ivec2 leftPosition = left(startPosition, endPosition);
	ivec2 rightPosition = right(startPosition, endPosition);

	while (leftPosition.x <= rightPosition.x) {
		if (getTile(leftPosition) != MAP_TILE_AIR) {
			return false;
		}

		int bottomTile = getTile(leftPosition + ivec2(0, 1));

		if (!isTileGround(bottomTile)) {
			return false;
		}

		leftPosition.x += 1;
	}

	return true;
}

bool Map::canFallFromGround(ivec2 startPosition, ivec2 endPosition)
{
	// End position lower that start position
	if (startPosition.y >= endPosition.y) {
		return false;
	}

	// Under start position is ground
	if (!this->isTileGround(getTile(startPosition + ivec2(0, 1)))) {
		return false;
	}

	ivec2 fallDirection(1, 0);

	if (startPosition.x > endPosition.x) {
		fallDirection = ivec2(-1, 0);
	}

	startPosition += fallDirection;

	int offset = 0;

	while (startPosition.y <= endPosition.y) {
		if (getTile(startPosition + fallDirection * offset) != MAP_TILE_AIR) {
			return false;
		}

		offset++;

		if (offset > abs(startPosition.x - endPosition.x)) {
			offset = 0;
			startPosition.y++;
		}
	}

	return true;
}

bool Map::canFall(ivec2 startPosition, ivec2 endPosition)
{
	// End position lower that start position
	if (startPosition.y >= endPosition.y) {
		return false;
	}

	ivec2 fallDirection(1, 0);

	if (startPosition.x > endPosition.x) {
		fallDirection = ivec2(-1, 0);
	}

	int offset = 0;

	while (startPosition.y <= endPosition.y) {
		if (getTile(startPosition + fallDirection * offset) != MAP_TILE_AIR) {
			return false;
		}

		offset++;

		if (offset > abs(startPosition.x - endPosition.x)) {
			offset = 0;
			startPosition.y++;
		}
	}

	return true;
}

bool Map::canJumpOnWall(ivec2 startPosition, ivec2 endPosition)
{
	// Start position lower that end position
	if (startPosition.y <= endPosition.y) {
		return false;
	}

	// Distance by x coords not equal 1
	if (abs(startPosition.x - endPosition.x) != 1) {
		return false;
	}

	// Under start position is ground
	if (!this->isTileGround(getTile(startPosition + ivec2(0, 1)))) {
		return false;
	}

	// End position is air
	if (getTile(endPosition) != MAP_TILE_AIR) {
		return false;
	}

	// Max wall height for 1 jumps
	if (startPosition.y - endPosition.y > 6 * 1) {
		return false;
	}

	endPosition.y += 1;

	while (startPosition.y >= endPosition.y) {
		if (!this->isTileGround(getTile(endPosition))) {
			return false;
		}

		endPosition.y++;
	}

	return true;
}

void Map::buildGraph()
{
	graph = MapGraph (GameClient()->Collision()->GetWidth(), MapGraphLine (GameClient()->Collision()->GetHeight(), MapGraphCell(0)));


	auto clientData = GameClient()->m_aClients[GameClient()->m_aLocalIDs[g_Config.m_ClDummy]];
	int airJumpImpulse = clientData.m_Predicted.m_Tuning.m_AirJumpImpulse.Get(); // 1200
	int gravity = clientData.m_Predicted.m_Tuning.m_Gravity.Get(); // 50
	int airControlAccel = clientData.m_Predicted.m_Tuning.m_AirControlAccel.Get(); // 150
	int airControlSpeed = clientData.m_Predicted.m_Tuning.m_AirControlSpeed.Get(); // 500
	int airFriction = clientData.m_Predicted.m_Tuning.m_AirFriction.Get(); // 95
	int jumpHeight = ceil(airJumpImpulse / gravity / 2.25); // 2.25 подоброное значение

	// 10

	// Movement graph
	int movementCost = 1;

	for (int x = 0; x < GameClient()->Collision()->GetWidth(); x++)
	{
		for (int y = 0; y < GameClient()->Collision()->GetHeight(); y++)
		{
			if (getTile(x, y) != MAP_TILE_AIR) {
				continue;
			}

			if (!isTileGround(getTile(x, y + 1))) {
				if (getTile(x - 1, y) == MAP_TILE_AIR && isTileGround(getTile(x - 1, y + 1)) &&
				    getTile(x + 1, y) == MAP_TILE_AIR && isTileGround(getTile(x + 1, y + 1))
				) {
					graph[x][y].push_back(MapGraphNode(x - 1, y, movementCost));
					graph[x][y].push_back(MapGraphNode(x + 1, y, movementCost));
				}

				continue;
			}

			if (getTile(x - 1, y) == MAP_TILE_AIR && isTileGround(getTile(x - 1, y + 1))) {
				graph[x][y].push_back(MapGraphNode(x - 1, y, movementCost));
			}

			if (getTile(x + 1, y) == MAP_TILE_AIR && isTileGround(getTile(x + 1, y + 1))) {
				graph[x][y].push_back(MapGraphNode(x + 1, y, movementCost));
			}
		}
	}

	// Fall graph
	int fallCost = 1;

	for (int x = 0; x < GameClient()->Collision()->GetWidth(); x++)
	{
		for (int y = 0; y < GameClient()->Collision()->GetHeight(); y++)
		{
			if (getTile(x, y) != MAP_TILE_AIR) {
				continue;
			}

			if (getTile(x, y + 1) == MAP_TILE_AIR) {
				graph[x][y].push_back(MapGraphNode(x, y + 1, fallCost));
			}

			if (isTileGround(getTile(x, y + 1))) {
				if (getTile(x + 1, y) == MAP_TILE_AIR && getTile(x + 1, y + 1) == MAP_TILE_AIR) {
					graph[x][y].push_back(MapGraphNode(x + 1, y + 1, fallCost * 2));
				}

				if (getTile(x - 1, y) == MAP_TILE_AIR && getTile(x - 1, y + 1) == MAP_TILE_AIR) {
					graph[x][y].push_back(MapGraphNode(x - 1, y + 1, fallCost * 2));
				}
			}
		}
	}

	// Jump graph
	int jumpCost = 1;

	std::string valuesString = std::to_string(airControlAccel) + " " + std::to_string(airControlSpeed) + " " + std::to_string(airFriction);

	GameClient()->Console()->Print(0, "MAP", valuesString.c_str());

	for (int x = 0; x < GameClient()->Collision()->GetWidth(); x++)
	{
		for (int y = 0; y < GameClient()->Collision()->GetHeight(); y++)
		{
			if (getTile(x, y) != MAP_TILE_AIR) {
				continue;
			}

			if (!isTileGround(getTile(x, y + 1))) {
				continue;
			}

			int height = 1;
			int leftSideOffset = 1;
			int rightSideOffset = 1;

			while (height < jumpHeight && getTile(x, y - height) == MAP_TILE_AIR) {
				graph[x][y].push_back(MapGraphNode(x, y - height, height * jumpCost));

				int i = 1;
				while (i <= leftSideOffset && getTile(x - i, y - height) == MAP_TILE_AIR) {
					graph[x][y].push_back(MapGraphNode(x - i, y - height, height * jumpCost + movementCost * i + 1));

					i++;
				}

				if (i <= leftSideOffset) {
					leftSideOffset = i;
				}

				i = 1;
				while (i <= rightSideOffset && getTile(x + i, y - height) == MAP_TILE_AIR) {
					graph[x][y].push_back(MapGraphNode(x + i, y - height, height * jumpCost + movementCost * i + 1));

					i++;
				}

				if (i <= rightSideOffset) {
					rightSideOffset = i;
				}

				leftSideOffset++;
				rightSideOffset++;
				height++;
			}
		}
	}
}

//void Map::buildGraph()
//{
//	graph = MapGraph (GameClient()->Collision()->GetWidth(), MapGraphLine (GameClient()->Collision()->GetHeight(), MapGraphCell(0)));
//
//	for (int x = 0; x < GameClient()->Collision()->GetWidth(); x++)
//	{
//		for (int y = 0; y < GameClient()->Collision()->GetHeight(); y++)
//		{
//			if (getTile(x, y) != MAP_TILE_AIR) {
//				continue;
//			}
//
//			int cost = findGroundNear(ivec2(x, y));
//
//			if (cost == -1) {
//				cost = 300;
//			} else {
//				cost *= 10;
//			}
//
//			int groundBottom = findGroundBottom(ivec2(x, y));
//
//			if (groundBottom >= 0 && groundBottom <= 10 && cost > groundBottom) {
//				cost = groundBottom;
//			}
//
//			if (y > 0 && getTile(x, y - 1) == MAP_TILE_AIR) {
//				graph[x][y].push_back(MapGraphNode(x, y - 1, cost));
//			}
//
//			if (x > 0 && getTile(x - 1, y) == MAP_TILE_AIR) {
//				graph[x][y].push_back(MapGraphNode(x - 1, y, cost));
//			}
//
//			if (y + 1 < GameClient()->Collision()->GetHeight() && getTile(x, y + 1) == MAP_TILE_AIR) {
//				graph[x][y].push_back(MapGraphNode(x, y + 1, cost));
//			}
//
//			if (x + 1 < GameClient()->Collision()->GetWidth() && getTile(x + 1, y) == MAP_TILE_AIR) {
//				graph[x][y].push_back(MapGraphNode(x + 1, y, cost));
//			}
//		}
//	}
//}

std::vector<Node*> Map::aStar(ivec2 startPosition, ivec2 endPosition)
{
	if (isTileGround(getTile(endPosition))) {
		return std::vector<Node*>();
	}

	std::priority_queue<Node*, std::vector<Node*>, PriorityQueueCompare> queue;
	std::vector<Node*> open_list;

	Node start {startPosition.x, startPosition.y, 0, nullptr};
	Node goal {endPosition.x, endPosition.y, 0, nullptr};
	start.cost = manhattan_distance(&start, &goal);

	queue.push(&start);
	open_list.push_back(&start);

	while (!queue.empty()) {
		Node* current = queue.top(); queue.pop();

		if (*current == goal) {
			return construct_path(current);
		}

		for (const auto &next : graph[current->x][current->y]) {
			Node nextNode {next.connectionWith.x, next.connectionWith.y, 0, current};
			nextNode.cost = current->cost - manhattan_distance(current, &goal) + manhattan_distance(&nextNode, &goal) + next.cost;

			// check if node already exists in open_list
			auto it = std::find_if(open_list.begin(), open_list.end(), [&nextNode](Node* node){
				return *node == nextNode;
			});

			if (it == open_list.end() || (*it)->cost > nextNode.cost) {
				if (it != open_list.end()) {
					open_list.erase(it); // erase the old node from open list
				}
				// insert the new node to the open list
				open_list.push_back(new Node (nextNode));
				queue.push(open_list.back());
			}
		}
	}
	// No path found
	return std::vector<Node*>();
}

vec2 Map::convertWorldToUI(vec2 worldPos)
{
	return vec2(GameClient()->UI()->Screen()->w / 2, GameClient()->UI()->Screen()->h / 2) + ((worldPos - GameClient()->m_Camera.m_Center) / GameClient()->m_Camera.m_Zoom * 0.75);
}

vec2 Map::convertMapToUI(vec2 mapPos)
{
	return this->convertWorldToUI(mapPos * 32);
}

int Map::findGroundNear(ivec2 position)
{
	ivec2 start = position;
	int offset = 0;
	int step = 0;
	ivec2 direction(1, 0);

	while ((position.x <= 0 || position.y <= 0 || position.x >= Collision()->GetWidth() || position.y >= Collision()->GetHeight()) || !isTileGround(getTile(position)))
	{
		position += direction;
		step++;

		if (step > floor((float) offset / 2)) {
			step = 0;
			offset++;
			rotate(direction, 90);
		}

		if (abs(start.x - position.x) + abs(start.y - position.y) > 10) {
			return -1;
		}
	}

	return abs(start.x - position.x) + abs(start.y - position.y);
}

int Map::findGroundBottom(ivec2 position)
{
	ivec2 start = position;

	while (!isTileGround(getTile(position)))
	{
		position.y++;

		if (position.y >= Collision()->GetHeight()) {
			return -1;
		}
	}

	return position.y - start.y;
}
