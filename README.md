# 1. إزالة الحزم النصف مثبتة والتي تسبب الخطأ
sudo dpkg --remove --force-remove-reinstreq libwayland-dev libxkbcommon-dev

# 2. إنشاء وتثبيت حزمة وهمية لـ libwayland-dev
mkdir -p /tmp/dummy-wayland-dev/DEBIAN

cat <<EOF > /tmp/dummy-wayland-dev/DEBIAN/control
Package: libwayland-dev
Version: 1.24.0
Architecture: amd64
Maintainer: local-admin <admin@localhost>
Description: Dummy package to satisfy dependencies without overwriting custom build
EOF

dpkg-deb --build /tmp/dummy-wayland-dev

sudo dpkg -i /tmp/dummy-wayland-dev.deb

# 3. إنشاء وتثبيت حزمة وهمية لـ libxkbcommon-dev

mkdir -p /tmp/dummy-xkbcommon-dev/DEBIAN

cat <<EOF > /tmp/dummy-xkbcommon-dev/DEBIAN/control
Package: libxkbcommon-dev
Version: 1.6.0-1build1
Architecture: amd64
Maintainer: local-admin <admin@localhost>
Description: Dummy package to satisfy dependencies without overwriting custom build
EOF

dpkg-deb --build /tmp/dummy-xkbcommon-dev

sudo dpkg -i /tmp/dummy-xkbcommon-dev.deb

# 4. إصلاح باقي الحزم العالقة عبر apt
sudo apt --fix-broken install -y

sudo apt-mark hold libwayland-dev libxkbcommon-dev
