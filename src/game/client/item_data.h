#ifndef GAME_CLIENT_ITEM_DATA_H
#define GAME_CLIENT_ITEM_DATA_H

class IItemData
{
public:
	virtual ~IItemData() = default;
	virtual bool IsVisible(float ScreenX0, float ScreenY0, float ScreenX1, float ScreenY1, bool IsPredicted) const = 0;
};

#endif
