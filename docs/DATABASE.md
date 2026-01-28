## Importing the official DDNet Database

```sh
$ wget https://ddnet.org/stats/ddnet-sql.zip # (~2.5 GiB)
$ unzip ddnet-sql.zip
$ yay -S mariadb mysql-connector-c++
$ sudo mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
$ sudo systemctl start mariadb
$ sudo mysqladmin -u root password 'PW'
$ mysql -u root -p'PW'
MariaDB [(none)]> create database teeworlds; create user 'teeworlds'@'localhost' identified by 'PW2'; grant all privileges on teeworlds.* to 'teeworlds'@'localhost'; flush privileges;
# this takes a while, you can remove the KEYs in record_race.sql to trade performance in queries
$ mysql -u teeworlds -p'PW2' teeworlds < ddnet-sql/record_*.sql

$ cat mine.cfg
sv_use_sql 1
add_sqlserver r teeworlds record teeworlds "PW2" "localhost" "3306"
add_sqlserver w teeworlds record teeworlds "PW2" "localhost" "3306"

$ cmake -Bbuild -DMYSQL=ON -GNinja
$ cmake --build build --target DDNet-Server
$ build/DDNet-Server -f mine.cfg
```
