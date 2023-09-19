//
// Created by danii on 03.09.2023.
//

#include "HumanLikeMouse.h"
#include "game/client/gameclient.h"
#include <random>
#include <utility>

const double sqrt3 = std::sqrt(3);
const double sqrt5 = std::sqrt(5);

double random_double() {
	return static_cast<double>(rand()) / RAND_MAX;
}

int randomNumber(int from, int to)
{
	std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<int> dist(from, to);

	return dist(rng);
}

std::vector<Point> HumanLikeMouse::getPoints(int start_x, int start_y, int dest_x, int dest_y, double G_0, double W_0, double M_0, double D_0) {
	std::vector<Point> points_visited;
	points_visited.push_back({start_x, start_y});

	double v_x = 0;
	double v_y = 0;
	double W_x = 0;
	double W_y = 0;

	srand(static_cast<unsigned>(::time(nullptr)));

	while (true) {
		double dist = std::hypot(dest_x - start_x, dest_y - start_y);
		if (dist < 1)
			break;

		double W_mag = std::min(W_0, dist);

		if (dist >= D_0) {
			W_x = W_x / sqrt3 + (2 * random_double() - 1) * W_mag / sqrt5;
			W_y = W_y / sqrt3 + (2 * random_double() - 1) * W_mag / sqrt5;
		} else {
			W_x /= sqrt3;
			W_y /= sqrt3;
			if (M_0 < 3)
				M_0 = random_double() * 3 + 3;
			else
				M_0 /= sqrt5;
		}

		v_x += W_x + G_0 * (dest_x - start_x) / dist;
		v_y += W_y + G_0 * (dest_y - start_y) / dist;

		double v_mag = std::hypot(v_x, v_y);

		if (v_mag > M_0) {
			double v_clip = M_0 / 2 + random_double() * M_0 / 2;
			v_x = (v_x / v_mag) * v_clip;
			v_y = (v_y / v_mag) * v_clip;
		}

		start_x += v_x;
		start_y += v_y;

		int move_x = static_cast<int>(std::round(start_x));
		int move_y = static_cast<int>(std::round(start_y));

		if (points_visited.back().x != move_x || points_visited.back().y != move_y) {
			points_visited.push_back({move_x, move_y});
		}
	}

	return points_visited;
}

Point HumanLikeMouse::getCurrentMousePosition()
{
	Point point;

	point.x = this->m_pClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX;
	point.y = this->m_pClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY;

	return point;
}

void HumanLikeMouse::moveToPoint(Point* targetPoint, float moveTime, function<void()> onArrival)
{
	Point startPoint = HumanLikeMouse::getCurrentMousePosition();
	std::vector<Point> points = this->getPoints(startPoint.x, startPoint.y, targetPoint->x, targetPoint->y);
	this->countPointsInWay = points.size();

	while (!this->targetWay.empty()) {
		this->targetWay.pop();
	}

	for(auto point : points) {
		this->targetWay.push(point);
	}

	this->onArrival = std::move(onArrival);
	this->moveTime = moveTime;
}

void HumanLikeMouse::moveToPlayer(int id, float moveTime, function<void()> onArrival)
{
	if (id == this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]) {
		return;
	}

	Point point;
	vec2 playerPos = this->m_pClient->m_aClients[id].m_Predicted.m_Pos + vec2(randomNumber(-8, 8), randomNumber(-8, 8)); // TODO : CHANGE TO PROXIMITY RADIUS
	vec2 diff = (playerPos - this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]].m_Predicted.m_Pos) * ((float)(randomNumber(50, 200)) / 100);
	point.x = diff.x;
	point.y = diff.y;

	this->moveToPoint(&point, moveTime, std::move(onArrival));
}

void HumanLikeMouse::processMouseMoving()
{
	static float timer = Client()->LocalTime();
	static int moveIteration = 1;
	static Point prevPoint = this->getCurrentMousePosition();
	float timeToAim = this->moveTime / this->countPointsInWay;
	float speed = 10;

	if (this->targetWay.empty() || timer + (timeToAim / speed) * moveIteration >= Client()->LocalTime()) {
		return;
	}

	timer = Client()->LocalTime();
	Point point = this->targetWay.front();

	int toX = prevPoint.x + (point.x - prevPoint.x) / speed * moveIteration;
	int toY = prevPoint.y + (point.y - prevPoint.y) / speed * moveIteration;

	this->m_pClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX = toX;
	this->m_pClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY = toY;
	this->m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].x = toX;
	this->m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].y = toY;

	if (moveIteration == speed) {
		this->targetWay.pop();
		prevPoint = point;
		moveIteration = 1;

		if (this->targetWay.empty()) {
			this->onArrival();
		}
	} else {
		moveIteration++;
	}
}

void HumanLikeMouse::OnUpdate()
{
	this->processMouseMoving();

	if (this->m_pClient->m_GameWorld.GameTick() <= 0) {
		return;
	}

	auto localClientData = this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]];
//	this->m_pClient->aimHelper.predictLaserShoot(localClientData.m_Predicted.m_Id, localClientData.m_Predicted.m_Pos + this->m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy]);
}

bool HumanLikeMouse::isMoveEnded()
{
	return this->targetWay.empty();
}
