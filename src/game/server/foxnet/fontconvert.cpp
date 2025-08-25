#include <base/system.h>

bool IsLetterA(const char *pStr)
{
	const char *symbols[] = {
		"A", "ᴀ", "ᴬ", "ᵃ", "𝔸", "𝕒", "🄰", "🅐", "Ａ", "ａ", "Ⓐ", "ⓐ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterB(const char *pStr)
{
	const char *symbols[] = {
		"b", "ʙ", "ᴮ", "ᵇ", "𝔹", "𝕓", "🄱", "🅑", "Ｂ", "ｂ", "Ⓑ", "ⓑ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterC(const char *pStr)
{
	const char *symbols[] = {
		"c", "ᴄ", "ᶜ", "ℂ", "𝕔", "🄲", "🅒", "Ｃ", "ｃ", "Ⓒ", "ⓒ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterD(const char *pStr)
{
	const char *symbols[] = {
		"d", "ᴅ", "ᴰ", "ᵈ", "𝔻", "𝕕", "🄳", "🅓", "Ｄ", "ｄ", "Ⓓ", "ⓓ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterE(const char *pStr)
{
	const char *symbols[] = {
		"e", "ᴇ", "ᴱ", "ᵉ", "𝔼", "𝕖", "🄴", "🅔", "Ｅ", "ｅ", "Ⓔ", "ⓔ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterF(const char *pStr)
{
	const char *symbols[] = {
		"f", "ғ", "ᶠ", "𝔽", "𝕗", "🄵", "🅕", "Ｆ", "ｆ", "Ⓕ", "ⓕ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterG(const char *pStr)
{
	const char *symbols[] = {
		"g", "ɢ", "ᴳ", "ᵍ", "𝔾", "𝕘", "🄶", "🅖", "Ｇ", "ｇ", "Ⓖ", "ⓖ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterH(const char *pStr)
{
	const char *symbols[] = {
		"h", "ʜ", "ᴴ", "ʰ", "ℍ", "𝕙", "🄷", "🅗", "Ｈ", "ｈ", "Ⓗ", "ⓗ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterI(const char *pStr)
{
	const char *symbols[] = {
		"i", "ɪ", "ᶦ", "ᴵ", "ⁱ", "𝕀", "𝕚", "🄸", "🅘", "Ｉ", "ｉ", "Ⓘ", "ⓘ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterJ(const char *pStr)
{
	const char *symbols[] = {
		"j", "ᴊ", "ᴶ", "ʲ", "𝕁", "𝕛", "🄹", "🅙", "Ｊ", "ｊ", "Ⓙ", "ⓙ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterK(const char *pStr)
{
	const char *symbols[] = {
		"k", "ᴋ", "ᴷ", "ᵏ", "𝕂", "𝕜", "🄺", "🅚", "Ｋ", "ｋ", "Ⓚ", "ⓚ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterL(const char *pStr)
{
	const char *symbols[] = {
		"l", "ʟ", "ᴸ", "ˡ", "𝕃", "𝕝", "🄻", "🅛", "Ｌ", "ｌ", "Ⓛ", "ⓛ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterM(const char *pStr)
{
	const char *symbols[] = {
		"m", "ᴍ", "ᴹ", "ᵐ", "𝕄", "𝕞", "🄼", "🅜", "Ｍ", "ｍ", "Ⓜ", "ⓜ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterN(const char *pStr)
{
	const char *symbols[] = {
		"n", "ɴ", "ᴺ", "ⁿ", "ℕ", "𝕟", "🄽", "🅝", "Ｎ", "ｎ", "Ⓝ", "ⓝ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterO(const char *pStr)
{
	const char *symbols[] = {
		"o", "ᴏ", "ᴼ", "ᵒ", "𝕆", "𝕠", "🄾", "🅞", "Ｏ", "ｏ", "Ⓞ", "ⓞ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterP(const char *pStr)
{
	const char *symbols[] = {
		"P", "ᴘ", "ᴾ", "ᵖ", "ℙ", "𝕡", "🄿", "🅟", "Ｐ", "ｐ", "Ⓟ", "ⓟ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterQ(const char *pStr)
{
	const char *symbols[] = {
		"Q", "ᴏ̨", "ᑫ", "ℚ", "𝕢", "🅀", "🅠", "Ｑ", "ｑ", "Ⓠ", "ⓠ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterR(const char *pStr)
{
	const char *symbols[] = {
		"R", "ʀ", "ᴿ", "ʳ", "ℝ", "𝕣", "🅁", "🅡", "Ｒ", "ｒ", "Ⓡ", "ⓡ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterS(const char *pStr)
{
	const char *symbols[] = {
		"S", "ˣ", "ᔆ", "ˢ", "𝕊", "𝕤", "🅂", "🅢", "Ｓ", "ｓ", "Ⓢ", "ⓢ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterT(const char *pStr)
{
	const char *symbols[] = {
		"T", "ᴛ", "ᵀ", "ᵗ", "𝕋", "𝕥", "🅃", "🅣", "Ｔ", "ｔ", "Ⓣ", "ⓣ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterU(const char *pStr)
{
	const char *symbols[] = {
		"U", "ᴜ", "ᵁ", "ᵘ", "𝕌", "𝕦", "🅄", "🅤", "Ｕ", "ｕ", "Ⓤ", "ⓤ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterV(const char *pStr)
{
	const char *symbols[] = {
		"V", "ᴠ", "ⱽ", "ᵛ", "𝕍", "𝕧", "🅅", "🅥", "Ｖ", "ｖ", "Ⓥ", "ⓥ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterW(const char *pStr)
{
	const char *symbols[] = {
		"W", "ᴡ", "ᵂ", "ʷ", "𝕎", "𝕨", "🅆", "🅦", "Ｗ", "ｗ", "Ⓦ", "ⓦ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterX(const char *pStr)
{
	const char *symbols[] = {
		"X", "ˣ", "𝕏", "𝕩", "🅇", "🅧", "Ｘ", "ｘ", "Ⓧ", "ⓧ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterY(const char *pStr)
{
	const char *symbols[] = {
		"Y", "ʏ", "ʸ", "𝕐", "𝕪", "🅈", "🅨", "Ｙ", "ｙ", "Ⓨ", "ⓨ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsLetterZ(const char *pStr)
{
	const char *symbols[] = {
		"Z", "ᴢ", "ᶻ", "ℤ", "𝕫", "🅉", "🅩", "Ｚ", "ｚ", "Ⓩ", "ⓩ"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber0(const char *pStr)
{
	const char *symbols[] = {
		"0", "⁰", "𝟘", "⓿", "０", "⓪"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber1(const char *pStr)
{
	const char *symbols[] = {
		"1", "¹", "𝟙", "➊", "１", "①"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber2(const char *pStr)
{
	const char *symbols[] = {
		"2", "²", "𝟚", "➋", "２", "②"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber3(const char *pStr)
{
	const char *symbols[] = {
		"3", "³", "𝟛", "➌", "３", "③"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber4(const char *pStr)
{
	const char *symbols[] = {
		"4", "⁴", "𝟜", "➍", "４", "④"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber5(const char *pStr)
{
	const char *symbols[] = {
		"5", "⁵", "𝟝", "➎", "５", "⑤"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber6(const char *pStr)
{
	const char *symbols[] = {
		"6", "⁶", "𝟞", "➏", "６", "⑥"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber7(const char *pStr)
{
	const char *symbols[] = {
		"7", "⁷", "𝟟", "➐", "７", "⑦"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber8(const char *pStr)
{
	const char *symbols[] = {
		"8", "⁸", "𝟠", "➑", "８", "⑧"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

bool IsNumber9(const char *pStr)
{
	const char *symbols[] = {
		"9", "⁹", "𝟡", "➒", "９", "⑨"};
	for(const char *sym : symbols)
		if(str_find_nocase(pStr, sym))
			return true;
	return false;
}

const char *FontConvert(const char *pMsg)
{
	// ᴀʙᴄᴅᴇғɢʜɪᴊᴋʟᴍɴᴏᴘᴏ̨ʀsᴛᴜᴠᴡxʏᴢ
	// ᴬᴮᶜᴰᴱᶠᴳᴴᴵᴶᴷᴸᴹᴺᴼᴾᑫᴿᔆᵀᵁⱽᵂˣʸᶻ¹²³⁴⁵⁶⁷⁸⁹⁰
	// ᵃᵇᶜᵈᵉᶠᵍʰᶦʲᵏˡᵐⁿᵒᵖᑫʳˢᵗᵘᵛʷˣʸᶻ¹²³⁴⁵⁶⁷⁸⁹⁰
	// 𝔸𝔹ℂ𝔻𝔼𝔽𝔾ℍ𝕀𝕁𝕂𝕃𝕄ℕ𝕆ℙℚℝ𝕊𝕋𝕌𝕍𝕎𝕏𝕐ℤ𝟙𝟚𝟛𝟜𝟝𝟞𝟟𝟠𝟡𝟘
	// 𝕒𝕓𝕔𝕕𝕖𝕗𝕘𝕙𝕚𝕛𝕜𝕝𝕞𝕟𝕠𝕡𝕢𝕣𝕤𝕥𝕦𝕧𝕨𝕩𝕪𝕫𝟙𝟚𝟛𝟜𝟝𝟞𝟟𝟠𝟡𝟘
	// 🄰🄱🄲🄳🄴🄵🄶🄷🄸🄹🄺🄻🄼🄽🄾🄿🅀🅁🅂🅃🅄🅅🅆🅇🅈🅉1234567890
	// 🅐🅑🅒🅓🅔🅕🅖🅗🅘🅙🅚🅛🅜🅝🅞🅟🅠🅡🅢🅣🅤🅥🅦🅧🅨🅩➊➋➌➍➎➏➐➑➒⓿
	// ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ１２３４５６７８９０
	// ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ
	// ⒶⒷⒸⒹⒺⒻⒼⒽⒾⒿⓀⓁⓂⓃⓄⓅⓆⓇⓈⓉⓊⓋⓌⓍⓎⓏ①②③④⑤⑥⑦⑧⑨⓪
	// ⓐⓑⓒⓓⓔⓕⓖⓗⓘⓙⓚⓛⓜⓝⓞⓟⓠⓡⓢⓣⓤⓥⓦⓧⓨⓩ

	const char *c = pMsg;
	char aLetter[8];

	static char DecodedMsg[512] = "";
	mem_zero(DecodedMsg, sizeof(DecodedMsg));

	while(*c)
	{
		const char *pOld = c;
		int code = str_utf8_decode(&c);
		if(code == 0)
			break;
		int len = c - pOld;
		if(len > 0 && len < (int)sizeof(aLetter))
		{
			mem_copy(aLetter, pOld, len);
			aLetter[len] = 0;

			if(IsLetterA(aLetter))
				str_copy(aLetter, "a");
			else if(IsLetterB(aLetter))
				str_copy(aLetter, "b");
			else if(IsLetterC(aLetter))
				str_copy(aLetter, "c");
			else if(IsLetterD(aLetter))
				str_copy(aLetter, "d");
			else if(IsLetterE(aLetter))
				str_copy(aLetter, "e");
			else if(IsLetterF(aLetter))
				str_copy(aLetter, "f");
			else if(IsLetterG(aLetter))
				str_copy(aLetter, "g");
			else if(IsLetterH(aLetter))
				str_copy(aLetter, "h");
			else if(IsLetterI(aLetter))
				str_copy(aLetter, "i");
			else if(IsLetterJ(aLetter))
				str_copy(aLetter, "j");
			else if(IsLetterK(aLetter))
				str_copy(aLetter, "k");
			else if(IsLetterL(aLetter))
				str_copy(aLetter, "l");
			else if(IsLetterM(aLetter))
				str_copy(aLetter, "m");
			else if(IsLetterN(aLetter))
				str_copy(aLetter, "n");
			else if(IsLetterO(aLetter))
				str_copy(aLetter, "o");
			else if(IsLetterP(aLetter))
				str_copy(aLetter, "p");
			else if(IsLetterQ(aLetter))
				str_copy(aLetter, "q");
			else if(IsLetterR(aLetter))
				str_copy(aLetter, "r");
			else if(IsLetterS(aLetter))
				str_copy(aLetter, "s");
			else if(IsLetterT(aLetter))
				str_copy(aLetter, "t");
			else if(IsLetterU(aLetter))
				str_copy(aLetter, "u");
			else if(IsLetterV(aLetter))
				str_copy(aLetter, "v");
			else if(IsLetterW(aLetter))
				str_copy(aLetter, "w");
			else if(IsLetterX(aLetter))
				str_copy(aLetter, "x");
			else if(IsLetterY(aLetter))
				str_copy(aLetter, "y");
			else if(IsLetterZ(aLetter))
				str_copy(aLetter, "z");
			else if(IsNumber0(aLetter))
				str_copy(aLetter, "0");
			else if(IsNumber1(aLetter))
				str_copy(aLetter, "1");
			else if(IsNumber2(aLetter))
				str_copy(aLetter, "2");
			else if(IsNumber3(aLetter))
				str_copy(aLetter, "3");
			else if(IsNumber4(aLetter))
				str_copy(aLetter, "4");
			else if(IsNumber5(aLetter))
				str_copy(aLetter, "5");
			else if(IsNumber6(aLetter))
				str_copy(aLetter, "6");
			else if(IsNumber7(aLetter))
				str_copy(aLetter, "7");
			else if(IsNumber8(aLetter))
				str_copy(aLetter, "8");
			else if(IsNumber9(aLetter))
				str_copy(aLetter, "9");

			str_append(DecodedMsg, aLetter);
		}
	}
	return DecodedMsg;
}