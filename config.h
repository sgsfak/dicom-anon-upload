#ifndef CONFIG_H
#define CONFIG_H
#include <QString>

struct dcm_upload_config {
    int id_;
    QString site_id;
    QString pid_prefix;

    dcm_upload_config(): id_(0), site_id(""), pid_prefix("") {}

    void save_config();
    static int read_config(dcm_upload_config& cfg);
};

#endif // CONFIG_H
