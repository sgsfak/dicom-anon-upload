New users need to go to a website like this: https://bcrypt-generator.com to generate a Bcrypt hash (this website calls this functionality "Encrypt") and send to us with their selected username.

We keep an Sqlite database (`users.sqlite`) that contains the following schema:

```sql
CREATE TABLE users(username text primary key, hash text not null);
```

Example contents of the `users` table:
```sql
sqlite> SELECT * FROM users;
username    hash
----------  ------------------------------------------------------------
stelios     $2y$10$EvCPDFJaD8471NCmbR4S4O.QhBl30khbrPedZXk15skHxW7TaUYhO
manolis     $2a$12$J4MOnTg/UwbZWJehXuJrIePkIPZocEG9uHJ.ojwSRwlhTDgL7OF96
```

