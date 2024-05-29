#ifndef TOKEN_DATA_H
#define TOKEN_DATA_H
#include <QString>
#include <vector>

struct token_data {
    QString access_token;
    QString refresh_token;
    int expires_in;
};


struct user_info {
    QString user_id;
    QString name;
    std::vector<QString> groups;
public:
    user_info(): name{"anonymous"}
    {}
};

#endif // TOKEN_DATA_H
