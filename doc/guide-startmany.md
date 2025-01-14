## Setting up your Wallet

### Create New Wallet Addresses

1. Open the QT Wallet.
2. Click the Receive tab.
3. Fill in the form to request a payment.
    * Label: mn01
    * Amount: 1000 (optional)
    * Click *Request payment*
5. Click the *Copy Address* button

Create a new wallet address for each Masternode.

Close your QT Wallet.

### Send 1,000 0DYNC to New Addresses

Send exactly 1,000 0DYNC to each new address created above.

### Create New Masternode Private Keys

Open your QT Wallet and go to console (from the menu select Tools => Debug Console)

Issue the following:

```masternode genkey```

*Note: A Masternode private key will need to be created for each Masternode you run. You should not use the same Masternode private key for multiple Masternodes.*

Close your QT Wallet.

## <a name="masternodeconf"></a>Create masternode.conf file

Remember... this is local. Make sure your QT is not running.

Create the masternode.conf file in the same directory as your wallet.dat.

Copy the Masternode private key and correspondig collateral output transaction that holds the 1K CASH.

*Please note, the Masternode priviate key is not the same as a wallet private key. Never put your wallet private key in the masternode.conf file. That is equivalent to putting your 1,000 0DYNC on the remote server and defeats the purpose of a hot/cold setup.*

### Get the collateral output

Open your QT Wallet and go to console (from the menu select Tools => Debug Console)

Issue the following:

```masternode outputs```

Make note of the hash (which is your collaterla_output) and index.

### Enter your Masternode details into your masternode.conf file
[From the cash github repo](https://github.com/zero-dynamics/cash-core/blob/master/doc/masternode_conf.md)

The new masternode.conf format consists of a space separated text file. Each line consisting of an alias, IP address followed by port, Masternode private key, collateral output transaction id and collateral output index.
(!!! Currently not implemented: "donation address and donation percentage (the latter two are optional and should be in format "address:percentage")." !!!)

```
alias ipaddress:port masternode_private_key collateral_output collateral_output_index (!!! see above "donationin_address:donation_percentage" !!!)
```



Example:

```
mn01 127.0.0.1:44400 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0
mn02 127.0.0.2:44400 93WaAb3htPJEV8E9aQcN23Jt97bPex7YvWfgMDTUdWJvzmrMqey aa9f1034d973377a5e733272c3d0eced1de22555ad45d6b24abadff8087948d4 0 (!!! see above "7gnwGHt17heGpG9Crfeh4KGpYNFugPhJdh:25" !!!)
```

## Update cash.conf on server

If you generated a new Masternode private key, you will need to update the remote cash.conf files.

Shut down the daemon and then edit the file.

```sudo nano .cash/cash.conf```

### Edit the masternodepairingkey
If you generated a new Masternode private key, you will need to update the masternodepairingkey value in your remote cash.conf file.

## Start your Masternodes

### Remote

If your remote server is not running, start your remote daemon as you normally would.

I usually confirm that remote is on the correct block by issuing:

```cashd getinfo```

And compare with the official explorer at http://explorer.cashpay.io/chain/Cash

### Local

Finally... time to start from local.

#### Open up your QT Wallet

From the menu select Tools => Debug Console

If you want to review your masternode.conf setting before starting the Masternodes, issue the following in the Debug Console:

```masternode list-conf```

Give it the eye-ball test. If satisfied, you can start your nodes one of two ways.

1. masternode start-alias [alias_from_masternode.conf]. Example ```masternode start-alias mn01```
2. masternode start-many
