## Sale API

### Price table - shows the current price, and how many tokens left at this price

remaining is x100 - divide by 100 to get the number of tokens

10000000 remaining ==> 100000.00 HYPHA remaining at this price

- current_round_id:  refers to _id_ in the _rounds_ table
- hypha_usd: "1.20 USD": How many USD / Hypha. 1.2 means 1 Hypha == 1.2 USD
- remaining: how many tokens remaining at this price. Divide by 100 for number of tokens.




```
❯ cleos --print-request get table buy.hypha buy.hypha price
REQUEST:
---------------------
POST /v1/chain/get_table_rows HTTP/1.0
Host: 127.0.0.1:8888
content-length: 270
Accept: */*
Connection: close

{
  "json": true,
  "code": "buy.hypha",
  "scope": "buy.hypha",
  "table": "price",
  "table_key": "",
  "lower_bound": "",
  "upper_bound": "",
  "limit": 10,
  "key_type": "",
  "index_position": "",
  "encode_type": "dec",
  "reverse": false,
  "show_payer": false
}
---------------------
{
  "rows": [{
      "id": 0,
      "current_round_id": 0,
      "hypha_usd": "1.00 USD",
      "remaining": 10000000
    }
  ],
  "more": false,
  "next_key": ""
}

```

### Sold Table - shows how many tokens total were sold

volume values are x100

So sold 1000 means 10 tokens have been sold. 

This is becaause HYPHA has 2 digits of precision => 1234 == 12.34 HYPHA sold

```
cleos --print-request get table buy.hypha buy.hypha sold
REQUEST:
---------------------
POST /v1/chain/get_table_rows HTTP/1.0
Host: 127.0.0.1:8888
content-length: 269
Accept: /
Connection: close

{
  "json": true,
  "code": "buy.hypha",
  "scope": "buy.hypha",
  "table": "sold",
  "table_key": "",
  "lower_bound": "",
  "upper_bound": "",
  "limit": 10,
  "key_type": "",
  "index_position": "",
  "encode_type": "dec",
  "reverse": false,
  "show_payer": false
}
---------------------
{
  "rows": [{
      "id": 0,
      "total_sold": 0
    }
  ],
  "more": false,
  "next_key": ""
}
```

### Rounds - show all sales rounds

- id: id of the round - see price table
- max_sold: x100 sold tokens, see sold table
- hypha_usd: rate, see above

```
cleos --print-request get table buy.hypha buy.hypha rounds
REQUEST:
---------------------
POST /v1/chain/get_table_rows HTTP/1.0
Host: 127.0.0.1:8888
content-length: 271
Accept: */*
Connection: close

{
  "json": true,
  "code": "buy.hypha",
  "scope": "buy.hypha",
  "table": "rounds",
  "table_key": "",
  "lower_bound": "",
  "upper_bound": "",
  "limit": 10,
  "key_type": "",
  "index_position": "",
  "encode_type": "dec",
  "reverse": false,
  "show_payer": false
}
---------------------
{
  "rows": [{
      "id": 0,
      "max_sold": 10000000,
      "hypha_usd": "1.00 USD"
    },{
      "id": 1,
      "max_sold": 20000000,
      "hypha_usd": "1.10 USD"
    },{
      "id": 2,
      "max_sold": 30000000,
      "hypha_usd": "1.20 USD"
    },{
      "id": 3,
      "max_sold": 40000000,
      "hypha_usd": "1.30 USD"
    },{
      "id": 4,
      "max_sold": 50000000,
      "hypha_usd": "1.40 USD"
    },{
      "id": 5,
      "max_sold": 60000000,
      "hypha_usd": "1.50 USD"
    },{
      "id": 6,
      "max_sold": 70000000,
      "hypha_usd": "1.60 USD"
    },{
      "id": 7,
      "max_sold": 80000000,
      "hypha_usd": "1.70 USD"
    },{
      "id": 8,
      "max_sold": 90000000,
      "hypha_usd": "1.80 USD"
    }
  ],
  "more": false,
  "next_key": ""
}

```

## Price History

Shows historic prices - the last entry is the current price. 

Date shows when the price changed - date is in UTC

```
cleost --print-request get table buy.hypha buy.hypha pricehistory
REQUEST:
---------------------
POST /v1/chain/get_table_rows HTTP/1.0
Host: test.hypha.earth
content-length: 277
Accept: */*
Connection: close

{
  "json": true,
  "code": "buy.hypha",
  "scope": "buy.hypha",
  "table": "pricehistory",
  "table_key": "",
  "lower_bound": "",
  "upper_bound": "",
  "limit": 10,
  "key_type": "",
  "index_position": "",
  "encode_type": "dec",
  "reverse": false,
  "show_payer": false
}
---------------------
{
  "rows": [{
      "id": 0,
      "hypha_usd": "1.00 USD",
      "date": "2022-03-13T16:05:39.500"
    },{
      "id": 1,
      "hypha_usd": "1.10 USD",
      "date": "2022-03-13T16:17:07.000"
    }
  ],
  "more": false,
  "next_key": ""
}

```

## Pay History

A history of payments made

```
❯ cleost --print-request get table buy.hypha buy.hypha payhistory  
REQUEST:
---------------------
POST /v1/chain/get_table_rows HTTP/1.0
Host: test.hypha.earth
content-length: 275
Accept: */*
Connection: close

{
  "json": true,
  "code": "buy.hypha",
  "scope": "buy.hypha",
  "table": "payhistory",
  "table_key": "",
  "lower_bound": "",
  "upper_bound": "",
  "limit": 10,
  "key_type": "",
  "index_position": "",
  "encode_type": "dec",
  "reverse": false,
  "show_payer": false
}
---------------------
{
  "rows": [{
      "id": 0,
      "recipientAccount": "seedsuserccc",
      "paymentSymbol": "BTC",
      "paymentQuantity": "0.000005",
      "paymentId": "1e52352fbe556cb4179fa2e0e71e968c7af3f3cfcbc4c49008b7f0c60bb3d9c36",
      "multipliedUsdValue": 10000
    },{
      "id": 1,
      "recipientAccount": "seedsuserccc",
      "paymentSymbol": "BTC",
      "paymentQuantity": "0.000005",
      "paymentId": "2e52352fbe556cb4179fa2e0e71e968c7af3f3cfcbc4c49008b7f0c60bb3d9c36",
      "multipliedUsdValue": 1000000000
    },{
      "id": 2,
      "recipientAccount": "seedsuserccc",
      "paymentSymbol": "BTC",
      "paymentQuantity": "0.000005",
      "paymentId": "3e52352fbe556cb4179fa2e0e71e968c7af3f3cfcbc4c49008b7f0c60bb3d9c36",
      "multipliedUsdValue": 11000
    }
  ],
  "more": false,
  "next_key": ""
}

```