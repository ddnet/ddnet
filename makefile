build:
	cmake   ../ddnet                  \
		-DCMAKE_BUILD_TYPE=Release  \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DAUTOUPDATE=OFF            \
		-DANTIBOT=ON                \
		-DVIDEORECORDER=ON          \
		-DUPNP=ON                   \
		-DMYSQL=ON                  \
		-GNinja
		ninja
