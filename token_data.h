#ifndef TOKEN_DATA_H
#define TOKEN_DATA_H
#include <QString>

struct token_data {
    QString access_token;
    QString refresh_token;
    int expires_in;
};

#endif // TOKEN_DATA_H
