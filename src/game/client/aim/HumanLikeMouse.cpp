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

	auto pythonInput = GameClient()->pythonController.inputs[g_Config.m_ClDummy];

	if (pythonInput.m_TargetX != 0 || pythonInput.m_TargetY != 0) {
		point.x = pythonInput.m_TargetX;
		point.y = pythonInput.m_TargetY;
	} else {
		point.x = this->m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].x;
		point.y = this->m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].y;
	}

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
	this->endMoveTime = Client()->LocalTime() + moveTime;

	this->countIterationsForOnePoint = max((int) (moveTime / 0.1), 1);
	movingToUser = -1;
}

void HumanLikeMouse::moveToPlayer(int id, float moveTime, function<void()> onArrival)
{
	if (id == this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]) {
		return;
	}

	Point point;
	vec2 playerPos = this->m_pClient->m_aClients[id].m_Predicted.m_Pos + this->m_pClient->m_aClients[id].m_Predicted.m_Vel;// + vec2(randomNumber(-8, 8), randomNumber(-8, 8)); // TODO : CHANGE TO PROXIMITY RADIUS
	vec2 diff = (playerPos - this->m_pClient->m_aClients[this->m_pClient->m_aLocalIDs[g_Config.m_ClDummy]].m_Predicted.m_Pos);// * ((float)(randomNumber(50, 200)) / 100);
	point.x = diff.x;
	point.y = diff.y;

	this->moveToPoint(&point, moveTime, std::move(onArrival));
	movingToUser = id;
}

void HumanLikeMouse::processMouseMoving()
{
	static float timer = Client()->LocalTime();
	static int moveIteration = 1;
	static Point prevPoint = this->getCurrentMousePosition();

	float leftTimeToAim = this->endMoveTime - Client()->LocalTime();
	float leftIterations = this->targetWay.size() * countIterationsForOnePoint - moveIteration;
	float timeToAim = leftTimeToAim / leftIterations;

	if (this->movingToUser != -1 && !this->targetWay.empty()) {
		auto localPlayer = GameClient()->m_aClients[GameClient()->m_aLocalIDs[g_Config.m_ClDummy]];
		auto player = GameClient()->m_aClients[GameClient()->m_aLocalIDs[this->movingToUser]];
		auto lastPoint = this->targetWay.back();

		if (distance(vec2(lastPoint.x, lastPoint.y), player.m_Predicted.m_Pos - localPlayer.m_Predicted.m_Pos) > 14) {
			this->moveToPlayer(this->movingToUser, this->endMoveTime - Client()->LocalTime(), this->onArrival);
			moveIteration = 1;
			return;
		}
	}

	if (this->targetWay.empty()) {
		timer = Client()->LocalTime();
		moveIteration = 1;
		prevPoint = this->getCurrentMousePosition();
		return;
	}

	if (timer + timeToAim >= Client()->LocalTime()) {
		return;
	}

	timer = Client()->LocalTime();
	Point point = this->targetWay.front();

	int toX = prevPoint.x + (point.x - prevPoint.x) / countIterationsForOnePoint * moveIteration;
	int toY = prevPoint.y + (point.y - prevPoint.y) / countIterationsForOnePoint * moveIteration;

	this->m_pClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetX = toX;
	this->m_pClient->pythonController.inputs[g_Config.m_ClDummy].m_TargetY = toY;

	if (moveIteration == countIterationsForOnePoint) {
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
}

bool HumanLikeMouse::isMoveEnded()
{
	return this->targetWay.empty();
}

void HumanLikeMouse::removeMoving()
{
	while(!targetWay.empty()) {
		targetWay.pop();
	}
}
