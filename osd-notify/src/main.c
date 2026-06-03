#include <gtk/gtk.h>
#include "osd.h"
#include "notify.h"
#include "osd_sound.h"
#include "osd_udev.h"

int main(int argc, char *argv[]) {
    // تهيئة GTK
    gtk_init(&argc, &argv);

    g_print("🎨 Starting venom_gui (OSD + Notify)\n");

    // تهيئة نظام الصوت
    osd_sound_init();

    // تهيئة نظام udev لمراقبة الشاحن والـ USB
    osd_udev_init();

    // تهيئة نظام OSD للغات
    osd_init();

    // تهيئة نظام الإشعارات
    notify_init();

    // تشغيل حلقة GTK الأساسية المشتركة
    gtk_main();

    return 0;
}
