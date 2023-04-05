## draft Guide to setting up a single-node local test environment (Antelope Leap, v3.x)

_Note: this guide describes a history solution that doesn't actually work (yet?)_

For contract development you will need
* the Contract Development Toolkit (cdt) - compilation tools
* the Antelope Leap suite - nodeos, cleos, other utilities

With these two, you can write smart contracts, compile them, upload them to a locally-running Antelope node,
and execute them. However in order to observe the results of execution, you will want a "history" solution. 
A history solution provides a standard http API which most available block explorers can use to display
transaction history. In this guide we describe installing a history solution containing two components
* Chronicle - which reprocesses the nodeos SHiP output into a form suitable for storing in a local database
* Memento - which stores data in a database 
* Memento-api - which presents an http API compatible with block explorers _(not) ... yet?_


This guide is written for an Ubuntu 20.04 operating system. Software versions were current at time of writing.

### Install cdt

The cdt repository is https://github.com/AntelopeIO/cdt , and the README file describes installation.
```
wget https://github.com/AntelopeIO/cdt/releases/download/v3.1.0/cdt_3.1.0-amd64.deb
sudo apt install ./cdt_3.1.0-amd64.deb
```
### Install Leap

the Leap repository is https://github.com/AntelopeIO/leap , and the README file describes installation.
```
wget https://github.com/AntelopeIO/leap/releases/download/v3.2.3/leap_3.2.3-ubuntu20.04_amd64.deb
sudo apt install ./leap_3.2.3-ubuntu20.04_amd64.deb
```

At this point it is possible to verify the insalled version and start a local node.
Note that we launch the node with `--delete-all-blocks --delete-state-history` flags, which is appropriate
when doing unit tests and other short-term development tasks.
```
nodeos --full-version
```
```
nodeos -e -p eosio --plugin eosio::producer_plugin --plugin eosio::producer_api_plugin --plugin eosio::chain_api_plugin --plugin eosio::state_history_plugin --trace-history --chain-state-history --disable-replay-opts --plugin eosio::http_plugin  --access-control-allow-origin='*' --access-control-allow-headers "*" --contracts-console --http-validate-host=false --delete-all-blocks --delete-state-history --verbose-http-errors >> nodeos.log
```
Once you see that the node starts correctly, you will want to stop the process and restart it with redirected console output:
```
nodeos -e -p eosio --plugin eosio::producer_plugin --plugin eosio::producer_api_plugin --plugin eosio::chain_api_plugin --plugin eosio::state_history_plugin --trace-history --chain-state-history --disable-replay-opts --plugin eosio::http_plugin  --access-control-allow-origin='*' --access-control-allow-headers "*" --contracts-console --http-validate-host=false --delete-all-blocks --delete-state-history --verbose-http-errors >> nodeos.log 2>&1
```
You can check that the node's chain API is responding with
```
cleos get info
```
and see that blocks are being produced at 

https://eosauthority.com/?network=localtest&endpoint=http:%2F%2F127.0.0.1:8888&token_symbol=EOS

### Install chronicle

The chronicle repository is at https://github.com/EOSChronicleProject/eos-chronicle .

Install the package and set up systemd task
```
wget https://github.com/EOSChronicleProject/eos-chronicle/releases/download/v2.4/eosio-chronicle-2.4-Clang-11.0.1-ubuntu20.04-x86_64.deb
sudo apt install ./eosio-chronicle-2.4-Clang-11.0.1-ubuntu20.04-x86_64.deb

sudo cp /usr/local/share/chronicle_receiver\@.service /etc/systemd/system/
sudo systemctl daemon-reload
```

### Install memento

The memento repository is at https://github.com/Antelope-Memento/antelope_memento , and the README includes an example setup script.

Memento writes the chronicle output data to a mariadb (mysql type) database, and uses some perl modules.
```
sudo apt update
sudo apt install mariadb-server mariadb-client

sudo apt install -y cpanminus libjson-xs-perl libjson-perl libmysqlclient-dev libdbi-perl libwww-perl make gcc
sudo cpanm --notest Net::WebSocket::Server
sudo cpanm --notest DBD::MariaDB
```
Clone the memento repository and set up systemd task.
```
sudo git clone https://github.com/Antelope-Memento/antelope_memento.git /opt/antelope_memento
cd /opt/antelope_memento
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
```
Initialize the database
```
sudo sh sql/mysql/create_db_users.sh
sudo sh sql/mysql/create_memento_db.sh memento_local
```
Set up the writer task.
```
echo 'DBWRITER_OPTS="--id=1 --port=8806 --dsn=dbi:MariaDB:database=memento_local"' | sudo tee /etc/default/memento_local >/dev/null
sudo systemctl enable memento_dbwriter@local
sudo systemctl start memento_dbwriter@local
systemctl status memento_dbwriter@local
```
Create the chronicle config file
```
sudo mkdir -p /srv/memento_local1/chronicle-config
sudo tee /srv/memento_local1/chronicle-config/config.ini >/dev/null <<'EOT'
host = 127.0.0.1
port = 8080
mode = scan
skip-block-events = true
plugin = exp_ws_plugin
exp-ws-host = 127.0.0.1
exp-ws-port = 8806
exp-ws-bin-header = true
skip-table-deltas = true
skip-account-info = true
EOT

```
Since we're running our nodeos test instance without a long-term history, we don't have to
worry about importing the old history; we can just start the chronicle-receiver task.
```
sudo systemctl enable chronicle_receiver@memento_local1
sudo systemctl start chronicle_receiver@memento_local1
```

### Install memento-api

The memento-api repository is at https://github.com/Antelope-Memento/antelope_memento_api , and the README describes installation.

Prerequisites: install nodejs
```
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo bash -
sudo apt-get install nodejs
```
Clone the repository and install dependencies
```
sudo git clone https://github.com/Antelope-Memento/antelope_memento_api.git /opt/antelope_memento_api
cd /opt/antelope_memento_api
sudo npm clean-install
```
Setup systemd task
```
sudo cp systemd/memento_api\@.service /etc/systemd/system/
```
Set up environment file
```
sudo tee /etc/opt/memento_api_local.env >/dev/null <<'EOT'
SERVER_BIND_IP = 127.0.0.1
SERVER_BIND_PORT = 3003
MYSQL_DB_HOST = 127.0.0.1
MYSQL_DB_PORT = 3306
MYSQL_DB_USER = memento_ro
MYSQL_DB_PWD = memento_ro
MYSQL_DB_NAME = memento_local
CONNECTION_POOL = 10
DATABASE_SELECT = "MYSQL"
HEALTHY_SYNC_TIME_DIFF = 15000
API_PATH_PREFIX = v2/history
CPU_CORES = 2
MAX_RECORD_COUNT = 100
EOT

```
Launch the service
```
sudo systemctl enable memento_api@local
sudo systemctl start memento_api@local
```




