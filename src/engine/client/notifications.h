class Notifications
{
public:
	static void init();
	static void uninit();
	static void Notify(const char *pTitle, const char *pMsg);
};
