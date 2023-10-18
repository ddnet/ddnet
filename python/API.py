from typing import Any, Tuple, Callable, List, Optional


class Vector2:
    x: float
    y: float

    def __init__(self, x: float, y: float):
        pass

    def distance(self, vector: 'Vector2') -> float:
        pass

    def __add__(self, other: 'Vector2') -> 'Vector2':
        pass

    def __sub__(self, other: 'Vector2') -> 'Vector2':
        pass

    def __mul__(self, other: float) -> 'Vector2':
        pass

    def __div__(self, other: float) -> 'Vector2':
        pass


class Tee:
    pos: Vector2
    vel: Vector2
    hookPos: Vector2
    hookDir: Vector2
    hookTeleBase: Vector2
    hookTick: int
    hookState: int
    hookedPlayer: int
    activeWeapon: int
    isNewHook: bool
    jumped: int
    jumpedTotal: int
    jumps: int
    direction: int
    angle: int
    triggeredEvents: int
    id: int
    isReset: bool
    lastVel: Vector2
    colliding: int
    isLeftWall: bool
    isSolo: bool
    isJetpack: bool
    isCollisionDisabled: bool
    isEndlessHook: bool
    isEndlessJump: bool
    isHammerHitDisabled: bool
    isGrenadeHitDisabled: bool
    isLaserHitDisabled: bool
    isShotgunHitDisabled: bool
    isHookHitDisabled: bool
    isSuper: bool
    hasTelegunGun: bool
    hasTelegunGrenade: bool
    hasTelegunLaser: bool
    freezeStart: int
    freezeEnd: int
    isInFreeze: bool
    isDeepFrozen: bool
    isLiveFrozen: bool


class Player:
    useCustomColor: int
    colorBody: int
    colorFeet: int
    name: str
    clan: str
    country: int
    skinName: str
    skinColor: int
    team: int
    emoticon: int
    emoticonStartFraction: float
    emoticonStartTick: int
    isSolo: bool
    isJetpack: bool
    isCollisionDisabled: bool
    isEndlessHook: bool
    isEndlessJump: bool
    isHammerHitDisabled: bool
    isGrenadeHitDisabled: bool
    isLaserHitDisabled: bool
    isShotgunHitDisabled: bool
    isHookHitDisabled: bool
    isSuper: bool
    isHasTelegunGun: bool
    isHasTelegunGrenade: bool
    isHasTelegunLaser: bool
    freezeEnd: int
    isDeepFrozen: bool
    isLiveFrozen: bool
    angle: float
    isActive: bool
    isChatIgnore: bool
    isEmoticonIgnore: bool
    isFriend: bool
    isFoe: bool
    authLevel: int
    isAfk: bool
    isPaused: bool
    isSpec: bool
    renderPos: Vector2
    isPredicted: bool
    isPredictedLocal: bool
    smoothStart: Tuple[int, int]
    smoothLen: Tuple[int, int]
    isSpecCharPresent: bool
    specChar: Vector2
    tee: Tee

    def __init__(self, playerId: int):
        pass


def LocalID(clientId: int) -> int:
    pass


class Collision:
    @staticmethod
    def intersectLine(position0: Vector2, position1: Vector2) -> Tuple[int, Vector2, Vector2]:
        pass


class Console:
    @staticmethod
    def debug(message: str) -> None:
        pass


Direction = {"Left": -1, "None": 0, "Right": 1}


class Input:

    @staticmethod
    def move(direction: Direction) -> None:
        pass

    @staticmethod
    def jump() -> None:
        pass

    @staticmethod
    def hook(hook: bool) -> None:
        pass

    @staticmethod
    def fire() -> None:
        pass

    @staticmethod
    def setBlockUserInput(block: bool) -> None:
        pass

    @staticmethod
    def setTarget(position: Vector2) -> None:
        pass

    @staticmethod
    def setTargetHumanLike(position: Vector2, moveTime: float, onArrived: Callable[[], None] = None) -> None:
        pass

    @staticmethod
    def moveMouseToPlayer(playerId: int, moveTime: float, onArrived: Callable[[], None] = None) -> None:
        pass

    @staticmethod
    def isHumanLikeMoveEnded() -> bool:
        pass

    @staticmethod
    def getMousePosition() -> Vector2:
        pass

    @staticmethod
    def getTargetPosition() -> Vector2:
        pass

    @staticmethod
    def removeMoving() -> None:
        pass

    @staticmethod
    def setWantedWeapon(weapon: int):
        pass

    @staticmethod
    def reset():
        pass


class DummyInput:
    @staticmethod
    def move(direction: Direction) -> None:
        pass

    @staticmethod
    def jump() -> None:
        pass

    @staticmethod
    def hook(hook: bool) -> None:
        pass

    @staticmethod
    def fire() -> None:
        pass

    @staticmethod
    def setTarget(position: Vector2) -> None:
        pass

    @staticmethod
    def getMousePosition() -> Vector2:
        pass

    @staticmethod
    def getTargetPosition() -> Vector2:
        pass

    @staticmethod
    def setWantedWeapon(weapon: int):
        pass

    @staticmethod
    def enableControl(enable: bool) -> None:
        pass


class Time:
    @staticmethod
    def getLocalTime() -> float:
        pass

    @staticmethod
    def getGameTick(localPlayerId: int = None) -> int:
        pass


class Predict:
    @staticmethod
    def predictLaser(shooter: Player, shootPosition: Vector2) -> Tuple[List[Vector2], Optional[Player]]:
        pass


class Character:
    playerFlags: int
    health: int
    armor: int
    ammoCount: int
    weapon: int
    emote: int
    attackTick: int

    isActive: bool
    position: Vector2

    def __init__(self, playerId: int):
        pass


#######################################################
# For using events, just write in your code function. #
# for example:                                        #
#   def onUpdate():                                   #
#       ...your_code                                  #
#######################################################
class Events:
    @staticmethod
    def onUpdate() -> None:
        pass

    # If you want reject user input, return `True` in `onInput`
    @staticmethod
    def onInput(keyCode: int, flags: int, keyName: str) -> Optional[bool]:
        pass

    @staticmethod
    def onScriptStarted() -> None:
        pass

    @staticmethod
    def onScriptStopped() -> None:
        pass

    @staticmethod
    def getScriptName() -> str:
        pass
