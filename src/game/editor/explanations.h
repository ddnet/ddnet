#ifndef GAME_EDITOR_EXPLANATIONS_H
#define GAME_EDITOR_EXPLANATIONS_H

class CExplanations
{
public:
	enum class EGametype
	{
		NONE,
		DDNET,
		FNG,
		RACE,
		VANILLA,
		BLOCKWORLDS
	};
	static const char *Explain(EGametype Gametype, int Tile, int Layer);

private:
	static const char *ExplainDDNet(int Tile, int Layer);
	static const char *ExplainFNG(int Tile, int Layer);
	static const char *ExplainVanilla(int Tile, int Layer);
};

#endif
