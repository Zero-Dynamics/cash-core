**Dynamic CHANGELOG**
-------------------------

**Dynamic v2.5.0.0**


* [BDAP] Add BDAP audits with meta data transactions
* [BDAP] Add BDAP x509 Certificate CA, requests and approval transactions
* [Build] Bump Boost version to 1.70
* [Build] Upgrade to OpenSSL v1.1.1i
* [PoW] Change DAA parameters
* [RPC] Add getsubsidy RPC command
* [RPC] Add audits RPC commands
* [RPC] Add certificate RPC commands
* [Wallet] Disable wallet versions
* [Wallet] Fix wallet issue where too many keys were being created during rescans
* [Util] Fix add months to epoch functions
* [Params] Add checkpoints every 100,000 blocks
* [Doc] Update build documentation for Ubuntu 20.04
* Bump protocol version to 72500
* Bump client version to 2.5.0.0

**Dynamic v2.4.4.1**

* Use std::function and std::bind in scheduler instead of boost/std mix
* [Masternode] Do not call sentinel methods when spork is inactive
* [Masternode] Remove mncache.dat file


**Dynamic v2.4.4.0**

* [Qt] Update Splashscreen for v2.4.4.0
* [RPC] More user-friendly error message when partially signing
* [Qt] Translations Updated
* [BDAP] Show fee required in the insufficient funds error message
* [BDAP] Use new epoch to string format function in Qt
* [Tests] Add unit test for new add months to epoch function
* [Util] Refactor add months to epoch functions
* [Test] Add unit tests for new format ISO date and time
* [BDAP] Fix comparison between signed and unsigned warning
* [RPC] Fix masternode-list bug


**Dynamic v2.4.3.0**

* [BDAP] Remove second month overrun check
* Bump client and block version to 2.4.3.0


**Dynamic v2.4.2.0**

* [BDAP] Set maximum months stored in local accounts db
* [BDAP] Limit acount registration to 100 years
* [Mempool] Don't check if standard tx for BDAP txs in accept to mempool
* [BDAP] Remove maximum months for updated accounts
* [BDAP] Fix add months to block and epoch times
* [Util] Fix epoch to ISO date string conversion
* Bump client and block version to v2.4.2.0
* Increase minimum protocol to v2.4 (71000) or greater
* [Wallet] Check txout instead of entire tx for BDAP


**Dynamic v2.4.1.0**

* [Qt] Update/Add Languages
* [BDAP] Fix send BDAP transaction after removing credits from wallet GetBalance() method
* [Wallet] Prevent transactions that require Ed25519 keys if wallet needs upgrading. Notify user.
* [Wallet] Prevent ed25519 keypool crash when trying to erase from an empty set
* [FIX] Fix locked wallet upgrade issues.
* [RPC] Refactor all RPC code files in a seperate directory
* [LOG] Silence LogPrint when not finding Masternodes with votes
* [DHT] Return an empty JSON array when the denylink record is not found
* [HD] Move mnemonic wordlists to separate directory
* [FIX] Correct versioning for alert max version
* [Wallet] Remove BDAP credits from GetBalance method
* [BDAP] Fix send BDAP transaction after removing credits from wallet GetBalance() method


**Dynamic v2.4.0.0**

* [DHT] Add more UDP ports and fix saving key in wallet
* [BDAP] Update enum item names so it doesn't overlap with Windows macros
* Merge pull request #25 from duality-solutions/windows
* [DHT] Add parameter info to RPC call error
* [BDAP] More fixes to private key wallet saving
* Merge pull request #26 from duality-solutions/dht-fixes2
* [DHT] Fix put mutable value method
* [DHT] Fix salt parsing in the put save data method
* [DHT] Check signature and pubkey length before saving for put
* [DHT] Fix put by using fixed sizes for pubkey and signature. Use variable sizes for salt and value
* Merge pull request #27 from duality-solutions/fix-dht3
* [DHT] Fix session event logging and searching memory maps
* [DHT] Use multimap and implement locks session events
* Resolve merge conflicts with v2.4-WIP
* [DHT] Rework libtorrent event threads
* [DHT] Fix async get operations
* [DHT] Add function to remove old libtorrent events from memory multimap
* [DHT] Fix MacOSX build warnings
* Merge branch 'v2.4-WIP' of https://github.com/duality-solutions/dynamic into merge-public
* [DHT] Fix put BDAP data function for mutable hash table database
* [DHT] Add RPC command that returns all put message events in memory
* [DHT] Add RPC command that returns all get mutable events in memory
* [DHT] Fix blocking alert thread issue affecting puts
* Merge branch 'v2.4-WIP' of https://github.com/duality-solutions/dynamic into merge-public
* Merge pull request #29 from duality-solutions/fix-dht5
* Merge branch 'master' into merge-public
* Merge pull request #30 from duality-solutions/merge-public
* [DHT] Fix put wait for reponse
* [BDAP] Add dump bdap keys RPC command <dumpbdapkeys>
* [BDAP] Add RPC command to import BDAP account private keys
* Merge branch 'master' into fix-dht6
* Merge pull request #31 from duality-solutions/fix-dht6
* [BDAP] Fix build errors after merge conflicts
* Merge pull request #32 from duality-solutions/hd-seed
* [BDAP] Add RPC command to get local wallet accounts <mybdapaccounts>
* Merge branch 'master' into myaccounts
* Merge pull request #33 from duality-solutions/myaccounts
* Update testnet spork address
* Update print in ThreadSocketHandler
* Fixes
* Merge pull request #34 from duality-solutions/spencer
* Fix Strings
* Change uint256S to Int
* Merge pull request #35 from duality-solutions/spencer
* Merge pull request #36 from duality-solutions/chainwork
* Merge pull request #37 from duality-solutions/newtestnet
* [DHT] Revert CPubKey class and add AddCryptedDHTKey
* [DHT] Fix segfault when shutdown before libtorrent session init
* Add mnemonic import
* Improve menu title
* Update fluidmasternode.cpp
* Add second spork address to testnet
* [DHT] Fixes to wallet encryption for Ed25519 keys
* [DHT] Fix libtorrent event map for bootstrap event
* [DHT] Cleanup storage debug.log usage
* [DHT] Fix last wallet encryption issue for ed25519 keys
* Merge pull request #39 from duality-solutions/dht-keys
* [DHT] Fix segfault when saving BDAP hash table key
* Merge pull request #258 from duality-solutions/v2.4-WIP
* Resolve merge conflicts and fix mnemonic build error
* Merge pull request #41 from duality-solutions/skfix
* [BDAP] Move entries leveldb database into blocks directory
* Merge pull request #40 from duality-solutions/dht-keys2
* Fix non-standard and null data sign step and combine signatures
* [Fluid] Add boost thread include to fluid db code files
* Fix override warnings in keystore and crypter
* [BDAP] Update certificate class, refactor and add category
* [BDAP] Update link classes
* [BDAP] Stub linking db and RPC commands
* [BDAP] Refactor common utility functions to seperate file
* [BDAP] Add invite message to link request class
* [BDAP] Add generic script parsing for linking and domain entries
* Remove second spork address for testnet
* Add privatenet to chain parameters
* Merge pull request #46 from duality-solutions/privatenet
* Add privatenet seed nodes
* [BDAP] Rearrange operation codes
* [BDAP] Use op1 and op2 to get operation type
* [BDAP] Fix update entry issue
* [BDAP] Put users and groups in @public.bdap.io by default
* Merge pull request #49 from duality-solutions/bdap-opcodes
* Merge branch 'master' into bdap-updates
* [BDAP] Fix issues caused by previous merge conflicts
* Merge pull request #48 from duality-solutions/bdap-updates
* Updated LibTorrent submodule, synced with root repo
* [BDAP] Updates to link requests
* [BDAP] Check if link request pubkey already exists in the mempool
* [BDAP] Add shared pubkey to link requests
* Merge pull request #51 from duality-solutions/linking
* Merge pull request #50 from duality-solutions/fix-warnings
* Merge public v2.4-WIP with private repo
* Resolve merge conflicts with v2.4-WIP public repo
* Sync public repo v2.4-WIP changes
* Fix warnings for struct/class mismatch and missing overrides
* Merge pull request #52 from duality-solutions/merge-public4
* [BDAP] Fix link request from and to me functions
* Update ReadMe
* Update README.md
* Update README.md
* Merge pull request #54 from duality-solutions/ReadMeUpdate
* Merge pull request #55 from duality-solutions/merge-public4
* [BDAP] Fixes to linking and account db functions
* Move endif to correct place
* [BDAP] Rename entry channel to sidechain
* [BDAP] Add link accept RPC command
* [BDAP] Fixes to link consensus checks
* [DHT] Fill Ed25519 private keys with zeros in destructor
* [BDAP] Fixes to Leveldb link request and accept databases
* [BDAP] More fixes for link request and accept consensus
* [BDAP] Fix transaction display in Qt UI for account entries and links
* [Fluid] Add initial sovereign addresses for privatenet
* [BDAP] Use Instant Send when possible for BDAP txs
* [BDAP] Fix mybdapaccounts after adding links
* [BDAP] Add expire time and pubkey to user and group operation transaction
* [BDAP] Implement link pending RPC commands
* Sync with public v2.4-WIP branch 2019-01-10
* Merge pull request #57 from duality-solutions/merge-public6
* Fix Windows build issue for embedded libtorrent
* Fix Windows Qt resource file so it will build
* Update libtorrent synced with original repo on 2019-01-11
* Merge pull request #53 from duality-solutions/linking2
* Add MacOS framework CoreFoundation and SystemConfiguration to make
* Update locales and add fully translated NL file
* Merge branch 'master' of https://github.com/duality-solutions/Dynamic-private
* Stub BIP39 Wordlist files for zh_CN/zh_TW/fr/it/jp/ko/es
* fix final text for translations
* Remove 1 ?
* Merge pull request #58 from duality-solutions/locales
* Update documentation
* Merge pull request #59 from duality-solutions/readme
* Update Docs
* Merge pull request #60 from duality-solutions/readme
* Fix mismatched parameters to LogPrint
* Fix warning comparison between signed and unsigned integer expressions
* Merge pull request #62 from duality-solutions/dm-bugfixes
* Add BDAP to menubar and associated GUI
* [DHT] Change alert threading to avoid segfault
* [DHT] Change alert threading to avoid segfault
* [BDAP] Fix linking destination and source address for request/accept
* [BDAP] Fix registration days parameter for RPC commands
* Merge pull request #64 from duality-solutions/updates
* BDAP Whitepaper v0.1
* Update BDAP.md
* Update BDAP.md
* Add DHT RPC Calls and link to code location.
* Codify OP_RETURN
* Update wording
* Improve formatting
* update dht link
* link dynamic and sequence
* Link BDAP references to website
* Improve Layout
* Add a para to intro
* Add to technical information.
* Link LDAP
* Improve wording
* Improve wording
* Improve layout
* Remove DYN and link Dynamic
* Merge pull request #66 from duality-solutions/RPCWording
* Update BDAP.md
* Add BDAP AddUser modal, button event handlers and css
* Update BDAP.md
* Stub BDK section
* [BDAP] Implement version 0 (public) linking without encryption
* [DHT] Fix keymeta wallet issue when loading
* [BDAP] Add signature proof verification for link requests
* Update class info
* Fix formatting
* Stub further sections and tidy RPC sections
* Outline stubs
* Amend RPC section formatting to match
* Remove `
* Update libtorrent, allow larger mutable DHT entries (from 1000 to 5120 bytes)
* Update Spanish Translation
* Merge pull request #68 from duality-solutions/locales
* Update dynamic_es.ts
* Merge pull request #67 from duality-solutions/linking4
* [BDAP GUI] Implement List All Users in TableWidget
* [BDAP] Add ability to filter by users, groups or all in ListDirectories
* [BDAP] Prevent creating duplicate links
* [BDAP] Add completed link RPC command
* [BDAP] Improve link request and accept database structure
* [BDAP] Filter pending link RPC commands
* [BDAP] Add delete link request and accept RPC command
* [BDAP] Remove OP_RETURN data from delete domain account RPC
* [BDAP GUI] Implement List All Groups in TableWidget. WIP
* [BDAP GUI] Make BDAP Users table resize dynamically. WIP
* [BDAP] Add create raw account transaction RPC command
* [BDAP GUI] Make BDAP Groups table resize dynamically. Bugfixes. WIP
* [BDAP GUI] Handle get user details. WIP
* [BDAP GUI] Change ListDirectories output from condensed to full in order to populate expiration date
* [BDAP GUI] Rearrange GUI. WIP
* [BDAP GUI] Change refresh method, add My Users/Groups checkbox. WIP
* Update importmnemonic RPC command example
* [BDAP GUI] Tweak GUI, wire up My Groups checkbox. WIP
* [BDAP] Add optional accountType parameter to RPC mybdapaccounts
* [BDAP] Check if account exists in createrawbdapaccount RPC
* [BDAP] Add RPC command to pay, sign and send a raw hex account
* [BDAP] Fix mybdapaccounts help
* [BDAP] Fix help for account RPC commands
* Merge pull request #70 from duality-solutions/rpc-bdap
* [BDAP] Fix DHT RPC comand help output
* [BDAP] Add parameter names to all bdap RPC commands
* [BDAP] Fix delete link consensus and wallet checks
* Merge pull request #69 from duality-solutions/linking5
* [BDAP GUI] Wire up search functionality. Save sort states.
* [BDAP] Fix issue when trying to delete link multiple times
* [BDAP GUI] Add BDAP Account Detail dialog box functionality
* Less chatty debug.log file when not in debug mode
* Add syncstatus RPC command for better info on chain status
* Merge pull request #71 from duality-solutions/sync-status
* Fix debug logging due to an extra category passed
* Tweak syncstatus progress calculation
* [BDAP GUI] Add User Dialog functionality. WIP
* Merge branch 'master' into qt-mockups
* [BDAP GUI] Tweak Add User Error Message reporting. WIP
* [BDAP GUI] Display user details upon successful add user transaction
* [BDAP GUI] Make My Users/Groups default view. Add record count display.
* [BDAP GUI] Add add group functionality. Enable wordWrap for longer error messages.
* [BDAP] Reduce linking logging when not in debug mode
* [BDAP GUI] Add delete user/group functionality
* [BDAP GUI] Add update BDAP user/group functionality
* [BDAP] Add VGP encryption library to build
* [BDAP] Add VGP encryption to link request
* [BDAP] Update VGP submodule to most recent master branch
* [BDAP] Add function to convert hex ed25519 pubkeys to bytes
* [BDAP] Add, remove, get version number for encrypted data
* [BDAP] Fix missing operation code translation
* [BDAP] Decrypt v1 link data before adding to local wallet
* [BDAP] Increase maximum link data size to 1592
* [BDAP] Add more debug log print for links
* Merge pull request #74 from duality-solutions/VGP
* [BDAP GUI] Add user-friendly behavior, more error handling and cleanup
* [BDAP GUI] Implement changes requested by PR reviewers
* Merge pull request #73 from duality-solutions/qt-mockups
* [BDAP] Update VGP library's gitignore
* [VGP] Fix unit test and qt test make files
* [BDAP QT] Remove net include from tablemodel class
* Update German Translation
* Stub BDAP UI translations to NL ts file
* Pass placeholdertext through QObject::tr
* [BDAP] Fix issue with deleting an account
* [BDAP] Fix decrypting link request and accept data
* [BDAP] Add missing license headers to code files
* [Fluid] Fix get miner reward after first change
* [Fluid] Move operations code to fluid directory
* Bump client version to 2.4.6
* Merge pull request #76 from duality-solutions/BDAP-Translate
* Fixes for PS and UTXO sorting
* Cleanup fee section
* Increase Signatures Required and Total
* Fix psq/psa conditions
* Amend GetCollateralAmount and GetMaxCollateralAmount
* Update denom info in locale
* c++14 for travis
* Increade skip DNS Thread
* Use IN6ADDR_ANY_INIT instead of in6addr_any
* Update select coin methods
* Call InitializeCurrentBlockTip after importing has finished
* Fix crash bug with duplicate inputs within a transaction
* Add instantlock field to getrawtransaction rpc output
* Use VersionBitsState instead of VersionBitsTipState to avoid cs_main lock
* Remove unused instantsenddepth from init
* Do not hold cs_main while emitting messages in WalletModel::prepareTransaction
* Base psq/pstx thresholold on the total number of up to date masternodes
* Workaround for MacOS Mojave Dark Mode
* Rename mnTimer to DnTimer
* BIP147
* Fix gettxoutsetinfo RPC command
* Update README.md
* Fix frameFeeSelection CSS
* Fix Mining Page Colour
* Remove auto entry of ports to config file
* Remove unused CheckWork function
* Fix ZMQAbstractNotifier
* Update Icons and Splashscreen
* Update TestNet checkpoint and minchainwork
* Light Purple for LockedbyInstantSend
* Update TestNet seed IP's
* Update Alert/Spork for TestNet
* Update mainnet minchainwork
* Add a column for IS lock status on Transactions tab
* fixes for shutdown sequence
* Automatic InstantSend locks for simple transactions and some fixes
* [ZMQ] Notify when an IS double spend is attempted
* Remove dummy confirmations in RPC API and GUI for InstantSend transactions
* Update showMNConfEditor -> showDNConfEditor
* Change default build to disable GPU miner
* Do not ignore patches in depends
* Force fvisibility=hidden when compiling on macos
* PrivateSend spending txes should have "outgoing" icon on overview screen
* Relay txes through DN network faster than through regular nodes
* Update XTHIN code
* Reduce Memory Imprint on DB
* Add protected distructor for CValidationInterface
* Amend DEFAULT_TRANSACTION_MAXFEE
* Fix ' warning: delete called on non-final 'PeerLogicValidation' that has virtual functions but non-virtual destructor'
* Add WakeMessageHandler call to UpdateBlockTip
* Cleanup from changing "boost::lexical_cast<int>" with atoi
* clang-format
* Update info for testnet spork key
* add missing file to makefile
* Remove bdap from DIST_SUBDIRS in make file
* Do not check for CUDA as default at configure time
* Repair Send in ThreadSocketHandler
* Repair nActiveStatePrev
* Repair 'GUI: QColor::setRgb: RGB parameters out of range' warning
* Fix segfault with nRefCount in net
* Amend Check in masternode.cpp
* minor things i missed
* update sync.cpp/h
* amend some versioning
* Use unique_ptr for db copy
* Use unique_ptr for wallet db env
* Remove dummy constructor
* Allow users to mix up to 16 rounds
* Only allow 2.4 nodes to mix with 2.4 nodes or newer
* remove unused variable from serialization
* A couple of small fixes for mixing collaterals
* Add an option to disable popups for PS mixing txes
* Identify PS collateral payments in transaction list a bit more accurate
* Add more variance to coin selection in PS mixing
* Revert Require all participants to submit equal number of inputs
* Split PS into Manager and Session and allow running multiple mixing sessions in parallel
* Document Sporks
* M-of-N-like sporks
* Update ImportPrivKey command
* adds rpc calls for and 'setprivatesendamount'
* Add versioning to spork cache
* extract sporkmanager from sporkmessage 
* Save/load spork cache
* Update HD Feature Base and update build-debian.md
* Sweep and Update RPC files
* Tidy chainparams and update block.cpp/h
* [GPU] Fix debug block info
* Update rpcwallet.cpp calls and add missing AbandonTransaction bits
* Update miner code back with new pointers etc.
* [GPU] Disable GPU info on configure failure 
* Fix Spork Address for TestNet
* Improve fee warning colour
* Update Qt and fix usage of dustRelayFee
* Fix segfault when sending a Qt transaction
* Increase fileout version in WriteFeeEstimates
* Amend Fee Structure
* Protect CSporkManager with critical section
* Fix wallet lock check in DoAutomaticDenominating
* Move block template specific stuff from CBlock to CBlockTemplate
* Fix activeMasternode task scheduler
* Show some info about the wallet dumped via dumpwallet and show warning
* Make sure pwalletMain is not null whenever it's used in PS client
* Update rpcmasternode and add helpers
* Switch RequestGovernanceObjectVotes from pointers to hashes
* remove/update dns seeders
* [GPU] Update obsolete macro AC_HELP_STRING
* [GPU] Prevent crash when changing the slidebar too fast
* [GPU] Optimise GPU Miner
* Fix copy elision warning in opencl device.cpp
* Supress OSX private field warnings
* Fix comparator and supress warning
* Supress MacOS build warnings for deprecated code
* [Qt] Fix styling of PrivateSend option on sendcoinsdialog.ui
* Increase Min Peer Protocol Version to 70900(v2.3) for Peers/MasternodePayments/InstantSend/PrivateSend
* [GPU] Fix GPU found block nonce before ProcessFoundSolution
* [GPU] Update README for GPU Mining
* [GPU] Seperate CPU and GPU miners to fix hashmeters
* [GPU] Fix Qt mining page layout so it expands
* [GPU] Fixes to miner UI controls
* [GPU] Hide slider until sync completes/change batch size
* [GPU] Split CPU/GPU UI thread allocation
* [GPU] fix global and order members as they appear in initializer
* [GPU] fix processingUnit batch size and minor code tweaks
* [GPU] fix autoreplace
* [GPU] move implementations out of definitions
* [GPU] fix const char* instead of std::string
* [GPU] fix unique_ptr null check
* [GPU] remove redundant comments and whitespace
* [GPU] fix thread names
* [GPU] remove redundant comments and whitespace
* [GPU] follow pattern and add this->
* [GPU] remove redundant DynamicMiner function
* [GPU] explicit boost::none for boost::bind
* [GPU] fix boost::optional no value_or method error
* [GPU] reorder constructor initialization
* [GPU] move constructor code to base
* [GPU] move hashTarget to protected variables
* [GPU] fix non-pointer operand
* [GPU] move protected variables
* [GPU] fix CPUMiner constructor
* [GPU] track chain tip
* [GPU] remove name collison
* [GPU] add device index to base class
* [GPU] provide pblock to LoopChecks
* [GPU] remove unused template from BaseMiner
* [GPU] remove static from StartLoop
* [GPU] c++11 compatibility
* [GPU] cleanup thread constructor
* [GPU] remove unused assignment
* [GPU] nullptr comparisons
* [GPU] use boost::shared_ptr as expected
* [GPU] move transactionsUdatedLast to base class
* [GPU] enable_gpu ifdef
* [GPU] fix miner start loop call
* [GPU] rename namespace to miners
* [GPU] move threads to namespace
* [GPU] fix ptr ptr semantics
* [GPU] fix count hashes function params
* [GPU] mark function as override
* [GPU] bring back enable_gpu ifdef
* [GPU] rename processing unit
* [GPU] remove static from thread_group
* [GPU] fix optional dereference
* [GPU] rename check function and fix breaks
* [GPU] use shared_ptr to manage thread_group ptr
* [GPU] Refactor miners code
* [GPU] Process hashes in batches
* [GPU] Remove obvious type hint
* [GPU] Device get total memory method
* [GPU] OpenCL kernel static build rule
* [GPU] Remove unsigned and signed int comparison
* [GPU] Mac build fix attempt
* [GPU] Disable CUDA in Travis
* [GPU] Fix autoconf warnings
* [GPU] Apple OpenCL linker flags
* [GPU] Implement GPU Mining for both Daemon/Qt
* [GPU] Split CUDA and OpenCL libraries
* [GPU] Add CUDA/OpenCL Libraries for GPU Mining
* Remove unused serialization method
* Fix consts in txmempool
* [depends] upgrade boost to 1.67.0
* [depends] upgrade dbus to 1.13.4
* [depends] update expad download path
* [depends] upgrade freetype to 2.9.1
* [depends] update libevent download path
* [depends] upgrade libxcb to 1.13
* [depends] upgrade miniupnpc to 2.1
* [depends] upgrade native_biplist to 1.0.3
* [depends] upgrade native_ccache to 3.4.2
* [depends] update native_ds_store download path
* [depends] upgrade native_mac_alias to 2.0.7
* [depends] upgrade native_protobuf to 3.6.0
* [depends] update openssl to 1.0.1k - Add RISC-V support
* [depends] upgrade qt to 5.9.6
* [depends] upgrade qrencode
* [depends] upgrade xcb_proto to 1.13
* [depends] upgrade xtrans to 1.3.5
* [depends] upgrade zeromq to 4.2.5
* [depends] add fix_configure_mac.patch file
* [depends] add fix_no_printer.patch file
* [depends] update mac-qmake.conf patch file
* [BDAP] Add directory type function
* [BDAP] Implement add new entry validation checking
* [BDAP] Fix validation when syncing blocks from peers
* [BDAP] Implement directory entry list RPC command
* [BDAP] Add getdirectoryinfo RPC command
* [BDAP] Add leveldb update method to db wrapper class
* [BDAP] Fix validate input function; leveldb check not DOS
* [BDAP] Update getdirectoryinfo help parameter
* [BDAP] Implement update & delete entry tx validation checking
* [BDAP] Fix saving tx hash in leveldb bdap database
* [BDAP] Replace updatedirectory RPC command
* [BDAP] Fix getdirectoryinfo return when entry not found
* [BDAP] Fix expire time in RPC commands
* [BDAP] Refactor directory to domainentry
* [BDAP] Add domain entry link class for binding operations
* [BDAP] Refactor domain entry certificate and move to its own class
* [BDAP] Refactor domain entry checkpoints and move to its own class
* [BDAP] Refactor domain entry channel data and move to its own class
* [BDAP] Check if tx exists in memory pool for new domain entries
* [BDAP] Update RPC command names
* [BDAP] Check domain entry wallet address and encrypt pub key
* [BDAP] Remove SignWalletAddress in domain entry. Only use WalletAddress
* [BDAP] Check ownership in updatedomainentry RPC command
* [BDAP] Fix domain entry so it populates block height
* [BDAP] Add code to check domain entry's previous UTXO
* Remove duplicate mem pool check in wallet available coins
* [BDAP] Add link address to domain entry class
* [BDAP] Check previous wallet address for update and delete txs
* [BDAP] Add delete domain entry RPC command
* [BDAP] Allow blank link address and encrypt pub key for delete operations
* [Fluid] Fix segfault when running reindex
* Move makekeypair to BDAP and change testnet spork pub key
* [BDAP] Add identity and identity verification classes
* [BDAP] Add missing operation codes
* [BDAP] Adjust identity verification class
* [BDAP] Fix domain entry object type enum
* [BDAP] Add entry audit data class
* [BDAP] Add new public group entry RPC command
* [BDAP] Add list domain group entries RPC command
* [BDAP] Add update and delete public group RPC commands
* Corrected the documentation for Fluid RPC Calls
* [BDAP] Refactor general functions and fix get op type
* [BDAP] Improve transaction display in Qt UI
* Added more seednodes
* Remove double forcecompactdb arg
* [Fluid] Refactor fluid code and remove index database
* [BDAP] Remove fluid reference from domain entry
* [Fluid] Add mining update and mint leveldb databases
* [Fluid] Implement new fluid databases
* [Fluid] Fix get fluid sovereigns RPC
* [Fluid] Fixes to get last functions and RPC commands
* [Fluid] Fix consensus issues with new db code
* Drop delayed headers logic and fix duplicate initial headers sync
* replace boost iterators with for
* RPC: Add description for InstantSend-related fields of mempool entry
* RPC: fix wallet lock check in
* minor reformatting
* Remove explicit wallet lock in MasternodeList::StartAll()
* Do not create mnb until masternodeSync is finished
* Don't drop mnb's for outdated MN's
* Fix previous commit and fix 2 Spork issues
* PrepareDenominate fix
* Sync DN list and DNW list from 3 peers max
* Use correct protocol when serializing messages in reply to
* Bump Versioning
* Update cash_qt.m4 (Remove ability to build with Qt4)


**Dynamic v2.3.5.0**

* Fix crash bug with duplicate inputs within a transaction
* Reduces memory usage and blockchain size on disk
* Bump client version to v2.3.5 and minimum protocol version to 70800 (v2.2)
* [Fluid] Refactor fluid code and remove index database
* [Fluid] Add mining update and mint leveldb databases
* [Fluid] Implement new fluid databases
* [Fluid] Fix get fluid sovereigns RPC
* [Fluid] Fixes to get last functions and RPC commands
* [Fluid] Fix consensus issues with new db code
* Update changelog and cleanup fluid to do lists/comments


**Dynamic v2.3.0.0**

* Skip existing Masternodes connections on mixing
* Protect CKeyHolderStorage via mutex
* Fix Boost 1.66 Compatibility
* Net Overhaul and BTC Inlining
* Bump Versions/Protocol (Updated Masternodes must get a fresh "Start" Signal with the new Binaries)
* Update Tests
* util: Add ParseUInt32 and ParseUInt64
* [RPC] getmempoolancestors/getmempooldescendants
* [RPC] setnetworkactive
* httpserver: drop boost
* Deprecate GetWork()
* [Fluid] Update sovereign identities addresses for testnet
* Update guiutil.cpp
* [RPC] Fix gettxout
* Update ReadBlockFromDisk
* Fix Fluid Check
* First version of Debian build howto
* Formatting fixes, corrections
* Removed exec flags where unneeded
* trivial: remove unnecessary variable fDaemon
* Fix some locks in net_processing.cpp
* Tiny cleanup in configure.ac
* Fix init segfault where InitLoadWallet() calls ATMP before genesis
* remove new int and make i an unsigned int to supress build warning
* fix op order to append first alert
* Remove recommendedMinimum from SetMaxOutboundTarget
* [init, wallet] ParameterInteraction() if wallet enabled
* Specify Protobuf version 2 in paymentrequest.proto
* net: fix maxuploadtarget setting
* Trivial: UndoReadFromDisk works on undo files (rev), not on block files
* Move static global randomizer seeds into CConnman
* [init] Get rid of some ENABLE_WALLET
* Remove last reference to CWalletDB from accounting_tests.cpp/Remove pwalletdb parameter from CWallet::AddAccountingEntry/Add CWallet::ReorderTransactions and use in accounting_tests.cpp/Add CWallet::ListAccountCashCredit
* [Qt] RPC-Console: support nested commands and simple value queries
* [Wallet] remove unused ThreadFlushWalletDB from removeprunedfunds
* init: Get rid of fDisableWallet
* [qt] WalletModel: Expose disablewallet
* Do not set an addr time penalty when a peer advertises itself.
* Check for high-entropy ASLR
* Move AutoBackup initialization into CWallet::InitAutoBackup
* [mempool] Fix relaypriority calculation error
* [depends] Fix Qt compilation with Xcode 8
* [rpc] throw JSONRPCError when utxo set can not be read
* Remove unused statements in serialization
* cashd: Daemonize using daemon(3)
* Decouple GetConfigFile and ReadConfigFile from global mapArgs
* deprecate begin/end ptrs
* net: fix a few cases where messages were sent rather than dropped upon disconnection
* Trivial: RPC: getblockchaininfo help: pruneheight is the lowest, not highest, block
* Sync dynamic-tx with tx version policy
* RPC: Chainparams: Remove Chainparams::fTestnetToBeDeprecatedFieldRPC
* Kill insecure_random and associated global state
* Cleanup enums in protocol.h
* Add preciousblock RPC
* Report NodeId in misbehaving debug
* Remove InsecureRand
* Fix logging in PushInventory
* Actually honor fMiningRequiresPeers in getblocktemplate
* Remove extern
* Bump LevelDB/UniValue/secp256k1 versions
* RPC changeover to JSONRPCRequest
* Qt refactors to better abstract wallet access
* [RPC] Add ImportMulti
* [RPC] importmulti: Avoid using boost::variant::operator!=, which is only in newer boost versions
* Move coincontrol.h to walletfolder
* Remove unnecessary function prototypes
* Eliminating Inconsistencies in Textual Output
* Make connect=0 disable automatic outbound connections.
* Repair rpc_wallet_tests.cpp and remove unused variable in coins_tests.cpp to remove build warning
* Repair final instance of mem pool to mempool
* [RPC] Remove invalid explanation from wallet fee message
* Return useful error message on ATMP failure/Deprecate Test
* Fix Build Warnings
* [Qt] overhaul smart-fee slider, adjust default confirmation target
* keypoololdest denote Unix epoch, not GMT
* [qt] Return useful error message on ATMP failure
* Store mempool and prioritization data to disk
* Throw exception in gobject prepare when CommitTransaction fails
* Move CWalletDB::ReorderTransactions to CWallet
* [rpc] ParseHash: Fail when length is not 64
* Trivial: Explicitly pass const CChainParams& to LoadBlockIndexDB()
* [Wallet] Refactor wallet/init interaction (Reaccept wtx, flush thread)
* Change DEFAULT_TX_CONFIRM_TARGET from 2 to 10
* Declare wallet.h functions inline
* net: make a few values immutable, and use deterministic randomness for the localnonce
* Add common failure cases for rpc server connection failure
* Remove unused CTxOut::GetHash()
* new var DIST_CONTRIB adds useful things for packagers from contrib/ to EXTRA_DIST
* Use RelevantServices instead of node_network in AttemptToEvict and cleanup NodeEvictionCandidate
* Allow filterclear messages for enabling TX relay only.
* Use nPowTargetSpacing in SendCoinsDialog::updateGlobalFeeVariables
* qt: Use correct conversion function for boost::path datadir
* Hash P2P messages as they are received instead of at process-time
* Addition of mining tab
* Initialize variable to prevent compiler warning
* fix getnettotals RPC description about timemillis
* Remove redundant duplicate-input check from CheckTransaction
* Serialization simplification/optimisation
* fNetworkActive is not protected by a lock, use an atomic
* Unset fImporting for loading mempool
* net: don't send feefilter messages before the version handshake is complete
* Remove block-request logic from INV message processing
* [Qt] fix coincontrol sort issue
* getrawtransaction should take a bool for verbose
* Move -salvagewallet, -zap(wtx) to where they belong
* Do not fully sort all nodes for addr relay
* fix CreateTransaction error messages
* Add check to IsCollateralValid
* Credit should be CAmount
* Update bench.cpp
* remove unnecessary calls to CheckFinalTx
* Split up AppInit2 into multiple phases
* Daemonize after datadir lock
* Get rid of fServer flag
* Trivial refactor: Remove extern keyword from function declarations, as they are extern by default.
* SendMoney: use already-calculated balance
* Disable fee estimates for a confirm target of 1 block
* Return txid even if ATMP fails for new transaction
* Do not run functions with necessary side-effects in assert()
* Use EXIT_FAILURE when calling exit()
* Stop MasternodeBroadcast::Relay() when not synced
* Add missing locks to masternode.cpp
* Move over to Sentinel Ping from Watchdog
* Remove zero-fee transactions as an option
* Update miningpage for out of sync situation + add tooltips
* Add Sexy Sliders
* Remove unused declaration in masternodeman.cpp
* Add check to ensure that generatetoaddress doesn't function on Main or TestNet
* [Miner] check for masternode sync before mining
* Hash Rate Widget for Mining Page
* [Masternode] Remove lock in ReadBlockFromDisk
* Initial complete Korean translation added
* add include to enable wallet to be built disabled
* Fix Unlocking Error When Mixing
* Refactor and fix restart
* Fix segfault crash when shutdown the GUI in disablewallet mode
* Increase mempool expiry time to 2 weeks
* [CoinControl] Allow non-wallet owned change addresses
* Allow shutdown during LoadMempool, dump only when necessary
* Add IsArgSet, ForceSetArg, ForceSetMultiArgs, ForceRemoveArg & new critical section
* [bugfix] save feeDelta instead of priorityDelta in DumpMempool
* Add missing mempool lock for CalculateMemPoolAncestors
* Qt/Intro: Various fixes
* [net]Fix close socket loop
* Bugfix: ancestor modifed fees were incorrect for descendants
* Fix Masternode List
* Remove some locking in net.h/net.cpp
* Fix connectivity check in CActiveMasternode::ManageStateInitial
* Force Masternodes to have listen=1 and maxconnections to be at least DEFAULT_MAX_PEER_CONNECTIONS
* fix SelectCoinsByDenominations
* [Init] Avoid segfault when called with -enableinstantsend=0
* Use correct version for fee estimates db
* Fix args throughout wallet
* Remove AddRef call in CNode constructor and do AddRef in AcceptConnection
* Fix races, clean up args, move wallet backup dir check to wallet.cpp
* Added check for open() returning a NULL pointer.
* Limit IS quorums by updated MNs only
* Fix nStart warning and actually use it
* Fix LevelDB warning in leveldb/util/logging.cc
* Update univalue and secp256k1 libraries (June 2018)
* Bump masternodeman versionCMasternodeMan-Version to 2
* Bump CGovernanceManager version to 23 to signify v2.3
* Change MasternodeMode to MasternodeMode to avoid confusion
* [BDAP] Increase OP_RETURN relay size for larger DAP entries
* [Fluid] Fix getfluidhistoryraw RPC command
* [Fluid] Fix getfluidsovereigns RPC command
* [Fluid] Allow negative fluid minting amounts
* fix copy address, issue 157
* Privatesend->PrivateSend Instantsend->InstantSend
* Identified and Fixed many issues with Korean Translations.
* Update dynamic_find_bdb48.m4
* [Fluid] Fix send fluid tx display in Qt UI
* Inline Argon2d code with commit fba7b9a
* Update CHANGELOG


**Dynamic v2.2.0.0**

* Add dynamic address label to request payment QR code
* [RPC] Fix createrawtx sequence number unsigned int parsing
* [Qt] Bump to Qt5.6.1
* Stop treating importaddress'ed scripts as change
* inline further with bitcoin
* increase connection limits for outbound
* Remove old unused function
* [Qt] Add dbcache migration path
* util: Update tinyformat
* net: Ignore P2P messages
* Mempool: Use Consensus::CheckTxInputs direclty over main::CheckInputs
* [Wallet] Remove CWalletDB* parameter from CWallet::AddToWallet
* Make CWallet::fFileBacked private
* Clean up init of wallet
* Update Copyrights
* Bump Version and Copyright Year
* Update Proto Version
* Update secp256k1
* Fix fixed seeds
* Update CHANGELOG


**Dynamic v2.1.0.0**

* [Trivial] Shift non-Fluid specific operations to separate file
* [Script] Remove OPCODES from non-existent features
* Add tags to mempool's mapTx indices
* remove unused NOBLKS_VERSION_{START,END} constants
* mempool: Re-remove ERROR logging for mempool rejects
* [Wallet] move wallet help string creation to CWallet
* Move GetTempPath() to testutil
* [Wallet] move 'load wallet phase' to CWallet
* use cached block hash in blockToJSON()
* [wallet] Move hardcoded file name out of log messages
* Mempool: Add tracking of ancestor packages
* De-neuter NODE_BLOOM
* Improve COutPoint less operator
* Correct importaddress help reference to importpubkey
* Implement feefilter P2P message
* Bump Versioning
* Prevent multiple calls to CWallet::AvailableCoins
* [RPC] Add generatetoaddress rpc to mine to an address
* Fix calculation of balances and available coins.
* Fix lockunspent help message
* [RPC] add missing abandon status documentation
* [RPC] Add import/removeprunedfunds rpc call
* [RPC] Rename masternodeprivkey->Masternode Pairing Key
* P2P: add maxtimeadjustment command line option
* masternodeprivkey->masternodeprivkey
* [Qt] remove trailing output-index from transaction-id
* [build-aux] Update Boost & check macros to latest serials
* Strip colour profiles from png's
* rpc: Register calls where they are defined
* [Wallet] refactor wallet/init interaction
* RPC: fix generatetoaddress failing to parse address and add unit test
* Fix no-wallet build after backports refactored RPCs
* Net: Add IPv6 Link-Local Address Support
* trivial: Globals: Explicitly pass const CChainParams& to ProcessMessage()
* Clean up lockorder data of destroyed mutexes
* Refactor IsRBFOptIn, avoid exception
* Only send one GetAddr response per connection.
* crypto: bytes counts are 64 bit
* Add missing new line
* Speed up getchaintips.
* Clean up warning/error handling
* qt: Add transaction hash to details window title & make it possible to show details of multiple transactions
* Log invalid block hash to make debugging easier.
* chain: define enum used as bit field as uint32_t
* Return from main instead of calling exit()
* tinyformat: force USE_VARIADIC_TEMPLATES
* util: switch LogPrint and error to variadic templates
* [trivial] Add missing const qualifiers.
* Create signmessagewithprivkey
* Improve rolling bloom filter performance and benchmark
* Fix insanity of CWalletDB::WriteTx and CWalletTx::WriteToDisk
* fReopenDebugLog and fRequestShutdown should be type sig_atomic_t
* Use SipHash-2-4 for various non-cryptographic hashes
* remove unneeded declaration and standardise
* Fix Socks5() connect failures to be less noisy and less unnecessarily scary
* [Wallet] Improve Wallet encapsulation
* remove unneeded logging
* Change mapRelay to store CTransactions
* Do not use mempool for GETDATA for tx accepted after the last mempool request
* Directly push messages instead of using CDataStream first
* Only use AddInventoryKnown for transactions
* Use std::atomic for fRequestShutdown and fReopenDebugLog
* Prevent multiple calls to ExtractDestination
* replace mapNextTx with slimmer setSpends
* Put back generation commands and implement Account Move
* Log/report in 10% steps during VerifyDB
* [RPC] Add support for sequence number
* Disable the mempool P2P command when bloom filters disabled
* Addrman offline attempts
* tor: Change auth order to only use HASHEDPASSWORD if -torpassword
* Remove CLIENT_DATE
* Revert BLOCK_DOWNLOAD_TIMEOUT_*
* Remove unnecessary call to AddInventoryKnown in INV message handling
* Fix crash on exit when -createwalletbackups=0
* introduced a fix for a instant send related edge case. Somehow the parameters got mixed up and fUseInstantSend was passed as iterations
* Add dynamic address label to request payment QR code
* [Qt] Bump to Qt5.6.1
* Stop treating importaddress'ed scripts as change
* increase connection limits for outbound
* Fix calls to AcceptToMemoryPool in PS submodules
* Improve handling of unconnecting headers
* Update CHANGELOG


**Dynamic v2.0.0.0**

* Fix Network Time Protocol (NTP)
* Introduce, OP_MINT, OP_REWARD_MASTERNODE and OP_REWARD_MINING opcode for Fluid Protocol
* Add string generation/parsing system to generate tokens for Fluid Protocol
* Set authentication keys for token generation to statically-defined addresses
* Update CBlockIndex and CChain models for storing Fluid Protocol derived variables
* Allow opcodes to carry token instruction and to detect tokens
* Implement derivation of token data into datasets
* Derive parameters (One-Time Reward, Masternode & PoW Reward) from datasets
* Implement token-history indexing and prevent replay attacks
* Change statically-defined addresses to identity-derived addresses (dynamic)
* Introduce RPC Calls maketoken, getrawpubkey, burndynamic, sendfluidtransaction, signtoken, consenttoken, verifyquorum, fluidcommandshistory, getfluidsovereigns
* Update secp256k1
* Remove block 300,000 fork data
* New Hash Settings
* Amend CPU Core Count
* Revert/Update and Strip Argon2d code
* Update LevelDB to 1.20
* Add Masternode checks to prevent payments until 500 are active
* Reduce nPowTargetTimespan to 1920 seconds
* Reduce nMinerConfirmationWindow to 30 blocks
* [Qt] Reduce a significant cs_main lock freeze 
* remove InstantSend votes for failed lock attemts after some timeout
* Fix mnp relay bug
* fix trafficgraphdatatests for qt4
* Fix edge case for IS (skip inputs that are too large)
* allow up to 40 chars in proposal name
* Multiple Fixes/Implement connman broadly
* Add more logging for DN votes and MNs missing votes
* Remove bogus assert on number of oubound connections.
* update nCollateralMinConfBlockHash for local (hot) masternode on dn start
* Fix sync reset on lack of activity
* fix nLastWatchdogVoteTime updates
* Fix bug: nCachedBlockHeight was not updated on start
* Fix compilation with qt < 5.2
* RPC help formatting updates
* Relay govobj and govvote to every compatible peer, not only to the one with the same version
* remove send addresses from listreceivedbyaddress output
* Remove cs_main from ThreadMnbRequestConnections
* do not calculate stuff that are not going to be visible in simple PSUI anyway & fix fSkipUnconfirmed
* Keep track of wallet UTXOs and use them for PS balances and rounds calculations
* speedup MakeCollateralAmounts by skiping denominated inputs early
* Reduce min relay tx fee
* more vin -> outpoint in masternode rpc output
* Move some (spamy) CMasternodeSync log messages to new log category
* Eliminate g_connman use in InstantSend module.
* Remove some recursive locks
* Fix masternode score/rank calculations (#1620)
* InstandSend overhaul & TXMempool Fixes
* fix TrafficGraphData bandwidth calculation
* Fix losing keys on PrivateSend
* Refactor masternode management
* Multiple Selection for peer and ban tables
* qt: Fixing division by zero in time remaining
* [qt] sync-overlay: Don't show progress twice
* qt: Plug many memory leaks
* [GUI] Backport Bitcoin Qt/Gui changes up to 0.14.x
* Fix Unlocked Access to vNodes
* Fix Sync
* Fix empty tooltip during sync under specific conditions
* fix SPORK_5_INSTANTSEND_MAX_VALUE validation in CWallet::CreateTransaction
* Eliminate g_connman use in spork module. 
* Use connman passed to ThreadSendAlert() instead of g_connman global.
* Fix duplicate headers download in initial sync
* fix off-by-1 in CSuperblock::GetPaymentsLimit
* fix number of blocks to wait after successful mixing tx
* Backport Bitcoin PR#7868: net: Split DNS resolving functionality out of net structures
* net: require lookup functions to specify all arguments to make it clear where DNS resolves are happening
* net: manually resolve dns seed sources
* net: resolve outside of storage structures
* net: disable resolving from storage structures
* net: No longer send local address in addrMe
* safe version of GetMasternodeByRank
* Do not add random inbound peers to addrman.
* Partially backport Bitcoin PR#9626: Clean up a few CConnman cs_vNodes/CNode things
* Delete some unused (and broken) functions in CConnman
* Ensure cs_vNodes is held when using the return value from FindNode
* Use GetAdjustedTime instead of GetTime when dealing with network-wide timestamps
* slightly refactor CPSNotificationInterface
* drop masternode index
* drop pCurrentBlockIndex and use cached block height instead (nCachedBlockHeight)
* add/use GetUTXO[Coins/Confirmations] helpers instead of GetInputAge[InstantSend]
* net: Consistently use GetTimeMicros() for inactivity checks 
* Fix MasternodeRateCheck
* Always good to initialise
* Necessary split of main.h to validation.cpp/net_processing.cpp
* Relay tx in sendrawtransaction according to its inv.type
* Fix : Reject invalid instantsend transaction
* fix instantsendtoaddress param convertion
* Fix potential deadlock in CInstantSend::UpdateLockedTransaction (#1571) 
* limit UpdatedBlockTip in IBD
* Pass reference when calling HasPayeeWithVotes
* Sync overhaul
* Make sure mixing messages are relayed/accepted properly
* backport 9008: Remove assert(nMaxInbound > 0)
* Backport Bitcoin PR#8049: Expose information on whether transaction relay is enabled in (#1545)
* fix potential deadlock in CMasternodeMan::CheckMnbAndUpdateMasternodeList
* fix potential deadlock in CGovernanceManager::ProcessVote
* add 6 to strAllowedChars
* Backport Bitcoin PR#8085: p2p: Begin encapsulation
* change invalid version string constant
* Added feeler connections increasing good addrs in the tried table.
* Backport Bitcoin PR#8113: Rework addnode behaviour (#1525) 
* Fix vulnerability with mapMasternodeOrphanObjects
* Remove bad chain alert partition check
* Fix potential deadlocks in InstantSend
* fix CDSNotificationInterface::UpdatedBlockTip signature to match the one in CValidationInterface
* fix a bug in CommitFinalTransaction
* fixed potential deadlock in CSuperblockManager::IsSuperblockTriggered
* Fix issues with mapSeenGovernanceObjects
* Backport Bitcoin PR#8084: Add recently accepted blocks and txn to AttemptToEvictConnection
* Backport Bitcoin PR#7906: net: prerequisites for p2p encapsulation changes
* fix race that could fail to persist a ban
* Remove non-determinism which is breaking net_tests
* Implement BIP69 outside of CTxIn/CTxOut 
* fix MakeCollateralAmounts
* Removal of Unused Files and CleanUp
* Further fixes to PrivateSend
* New rpc call 'masternodelist info'
* Backport Bitcoin PR#7749: Enforce expected outbound services
* Backport Bitcoin PR#7696: Fix de-serialization bug where AddrMan is corrupted after exception
* Fixed issues with propagation of governance objects and update governance
* Backport Bitcoin PR#7458 : [Net] peers.dat, banlist.dat recreated when missing
* Backport Bitcoin PR#7350: Banlist updates
* Replace watchdogs with ping 
* Update timedata.h
* Trivial Fixes
* Eliminate unnecessary call to CheckBlock 
* PrivateSend: dont waste keys from keypool on failure in CreateDenominated
* Refactor PS and fix minor build issues preventing Travis-CI from completing previously
* Fix Governance Test File
* Increase test coverage for addrman and addrinfo
* Backport Bitcoin PRs #6589, #7180 and remaining part of #7181
* Don't try to create empty datadir before the real path is known
* Documentation: Add spork message / details to protocol-documentation
* Validate proposals on prepare and submit
* Fix signal/slot in GUI
* Fix PS/IS/Balance display in SendCoinsDialog
* Make CBlockIndex param const
* Explicitly pass const CChainParams& to UpdateTip()
* Change Class to Struct/Change int to unsigned int
* Fix copy elision warning
* Fix comparison of integers of different signs in masternodeman
* Remove unused int
* Drop GetMasternodeByRank
* [GUI] Remove Multiple Signatures GUI from Client
* [DDNS] Remove DDNS and DynDNS System from Dynamic
* Fix Conflicts/Remove Files from qt.pro
* PrivateSend Refactor
* Enable build with --disable-wallet
* Update Logos
* Remove remaining usage of 'namespace std;'
* Fix missing initializer in ntp.cpp
* [Fluid] Add help and example to getfluidsovereigns command 
* Add undocumented -forcecompactdb to force LevelDB compactions
* Remove ability to run Hot/Local Masternodes
* [Fluid] Add fluid history RPC command in clear text 
* make CheckPSTXes() private, execute it on both client and server
* Use IsPayToPublicKeyHash
* upgrade qrencode 4.0.0
* Amend maketoken
* Fix SpendCoin in CCoinsViewCache
* upgrade mac alias 2.0.1
* upgrade ds store 1.1.2
* Suppress warning with GenerateRandomString
* Guard 'if' statement
* add params.size() !=1 to maketocken in rpcfluid
* upgrade protobuf 3.5.0
* upgrade ccache 3.3.4
* upgrade miniupnpc 2.0.20171102
* upgrade xcb proto 1.12
* upgrade xproto 7.0.31
* upgrade libxcb 1.12
* upgrade libXext 1.3.3
* upgrade libX11 1.6.5
* upgrade freetype 2.8.1
* Update fontconfig.mk
* upgrade expat 2.2.5
* Fix upgrade cancel warnings
* Force on-the-fly compaction during pertxout upgrade
* Allow to cancel the txdb upgrade via splashscreen keypress
* Address nits from per-utxo change
* Simplify return values of GetCoin/HaveCoin(InCache)
* Change semantics of HaveCoinInCache to match HaveCoin
* Few Minor per-utxo assert-semantics re-adds and tweak
* upgrade dbus 1.12.2
* Don't return stale data from CCoinsViewCache::Cursor()
* Switch chainstate db and cache to per-txout model 
* fix abs warnings
* Change boost usage in coins.h to standard
* remove InstantSend votes for failed lock attempts
* Fix some empty vector references
* Add COMPACTSIZE wrapper similar to VARINT for serialization
* Fix: make CCoinsViewDbCursor::Seek work for missing keys
* Simplify DisconnectBlock arguments/return value
* Make DisconnectBlock and ConnectBlock static in validation.cpp
* Clean up calculations of pcoinsTip memory usage
* Compensate for memory peak at flush time
* Plug leveldb logs to Dynamic logs
* Add data() method to CDataStream (and use it)
* Share unused mempool memory with coincache
* Assert FRESH validity in CCoinsViewCache::BatchWrite
* Fix dangerous condition in ModifyNewCoins.
* [Fluid] Check if fluid transaction is already in the memory pool
* boost 1.65.1
* [test] Add CCoinsViewCache Access/Modify/Write tests
* Batch construct batches
* Remove undefined FetchCoins method declaration
* Use fixed preallocation instead of costly GetSerializeSize
* Fix OOM when deserializing UTXO entries with invalid length
* Avoid unnecessary database access for unknown transactions
* Use C++11 thread-safe static initializers in coins.h/coins.cpp
* Use SipHash-2-4 for CCoinsCache index 
* Add missing int
* Add SipHash-2-4 primitives to hash
* Move index structures into spentindex.h
* Break circular dependency main ↔ txdb
* Minor changes to dbwrapper to simplify support for other databases
* Fix assert crash in new UTXO set cursor
* Add cursor to iterate over utxo set, use this in
* Save the last unnecessary database read
* fix nLastWatchdogVoteTime
* fix Examples section of the RPC output for listreceivedbyaccount, lis…
* [Fluid] Add fluid amount check to consensus validation
* Allow IS for all txes, not only for txes with p2pkh and data outputs
* add `maxgovobjdatasize` field to the output of `getgovernanceinfo`
* [Fluid] check if exceeds maximum fluid amount and negative amount.
* [DDNS] Remove existing dDNS code 
* Update verbiage in debug log and add missing ENABLE_WALLET comment
* [DebugLog] Fix block reward debug output logging
* [Fluid] Stub maximum fluid operation amounts
* Remove extraneous LogPrint from fee estimation
* fix a bug if the min fee is 0 for FeeFilterRounder
* Disable fee estimates for a confirm target of 1 block
* Remove priority estimation
* Kill insecure_random and associated global state
* [Fluid] Use ParseInt64 instead of new convert function
* [Fluid] Remove fee direction
* [Mining] Fix floating point accuracy when printing CreateNewBlock amount
* [Fluid] Remove fluid quorumcheck from debug.log file
* DELTA swapped for Digishield V3
* Fixed a bug where the DAA wasn't using the parameters set in chainparams
* Remove unused enum
* Remove unneeded check for enum
* Add CEO/CFO/COO/CDOO Sovereigns
* Make sure additional indexes are recalculated correctly in VerifyDB
* Remove global use of g_connman
* InstantSend txes should never qualify to be 0-fee txes
* rpc: Input-from-stdin mode for cash-cli
* Move RPC dispatch table registration to wallet/rpcwallet
* Switch to a more efficient rolling Bloom filter
* remove cs_main lock from
* Combine common error strings for different options so translations can be shared and reused
* Removed comment about IsStandard for P2SH scripts
* Fix typo, wrong information in gettxout help text.
* amend -? help message
* Improved readability of ApproximateBestSubset
* [Qt] rename 'amount' to 'requested amount' in receive coins table
* Reduce inefficiency of GetAccountAddress()
* GUI: Disable tab navigation for peers tables.
* limitfreerelay edge case bugfix
* Move non-consensus functions out of pow
* mempool: Replace maxFeeRate of (10000 x minRelayTxFee) with maxTxFee
* Move maxTxFee out of mempool
* include the chaintip blockindex in the SyncTransaction signal, add signal UpdateTip()
* Common argument defaults for NODE_BLOOM stuff and -wallet
* Move privatesend to rpcwallet.cpp
* Optimize CheckOutpoint
* Update CHANGELOG


**Dynamic v1.4.0.0**

* Securely erase potentially sensitive keys/values
* Fix issue where config was created at launch but not read
* [BUILD] quiet annoying warnings without adding new ones
* [BUILD]Fix warning deprecated in OSX Sierra
* Improve EncodeBase58/DecodeBase58 performance.
* Use hardware timestamps in RNG seeding
* Add OSX keystroke to clear RPCConsole
* Update DB_CORRUPT message
* HD Wallet
* Repair Traffic Graph
* Scammer Warning and Translations
* Amend DEFAULT_CHECKBLOCKS
* Do not shadow upper local variable 'send', prevent -Wshadow compiler warning
* Convert last boost::scoped_ptr to std::unique_ptr
* Qt: fix UI bug that could result in paying unexpected fee
* Fix Locks and Do not add random inbound peers to addrman
* Use std::thread::hardwarencurrency, instead of Boost, to determine available cores
* Sync icon now opens modaloverlay.ui
* Fix Memleak and Enforce Fix
* Sort dynamic.qrc
* Sort MakeFiles
* Remove namespace std;/Repair Tests
* Fix Signal/Slot/Strings
* Implement modaloverlay
* Qt: Sort transactions by date
* Kill insecure_random and associated global state
* Only send one GetAddr response per connection.
* Refactor: Removed begin/end_ptr functions.
* LevelDB 1.19
* Increase context.threads to 4
* Fix races
* Fix calculation of number of bound sockets to use
* Fix unlocked access to vNodes.size()
* Move GetAccountBalance from rpcwallet.cpp into CWallet::GetAccountBalance
* UpdateTip: log only one line at most per block
* VerifyDB: don't check blocks that have been pruned 
* qt: askpassphrasedialog: Clear pass fields on accept
* net: Avoid duplicate getheaders requests.
* Check non-null pindex before potentially referencing
* mapNextTx: use pointer as key, simplify value
* Implement indirectmap.h and update memusage.h
* Add/Repair LOCK's
* Fix parameter naming inconsistencies 
* Clicking on the lock icon will open the passphrase dialog
* Fix bip32_tests.cpp
* Update Argon2d, hash.cpp/h
* Repair SLOT issue in rpcconsole.cpp
* Fix incorrect psTx usages
* Fix torcontrol.cpp unused private field warning
* Update Encryption(crypter.cpp/h)
* Remove old HD wallet code
* Move InitLoadWallet to init.cpp
* Revert Tick Changes/Fix UI Thread Issue
* Sentinel/Masternode Fixes
* Remove unused functions/cleanup code
* Reduce Keypool to 1000
* Optimise Reindex
* Bump Governance/InstantSend/PrivateSend/Core Proto/Versions
* Update CHANGELOG


**Dynamic v1.3.0.2**

* [Sync] Fix issue with headers first sync
* [Sync] [Consensus] Shift Fork Logic to its own file
* [Qt] Add CheckForks in the Qt Project File
* [Fork] Silence usage of pindex compeletely
* [Sync]Timeouts/DB/Headers/Limits
* Reduce nDefaultDbCache to 512MiB
* Bump Proto and ONLY connect to 1.3.0.1 (Proto 70200)
* Bump Governance/Core Proto/Versions
* Update CHANGELOG


**Dynamic v1.3.0.1**

* Bump Protocols to lock out nodes at or below v1.2 to prevent any forks
* Update CHANGELOG


**Dynamic v1.3.0.0**

* c++11:Backport from bitcoin-core: don't throw from the reverselock destructor
* InitError instead of throw on failure
* Hard Fork at block 300,000 for Delta difficulty retarget algorithm
* Update CHANGELOG


**Dynamic v1.2.0.0**

* Make RelayWalletTransaction attempt to AcceptToMemoryPool
* Update tests for Byteswap
* Ensure is in valid range
* Make strWalletFile const
* Avoid ugly exception in log on unknown inv type
* libconsensus: Add input validation of flags/missing flags & make catch() args const
* Add LockedPool
* Add getmemoryinfo
* Add benchmark for lockedpool allocation/deallocation
* trivial: fix bloom filter init to isEmpty = true
* Lockedpool fixes
* Add include utility to miner.cpp
* Don't return the address of a P2SH of a P2SH
* Implement (begin|end)_ptr in C++11 and add deprecation comment
* add include stdlib.h to random.cpp
* Generate auth cookie in hex instead of base64
* Do not shadow variable (multiple files)
* cash-cli: More detailed error reporting
* Add High TX Fee Warning
* C++11: s/boost::scoped_ptr/std::unique_ptr/
* Do not shadow variables in networking code
* Remove shadowed variables in Qt files
* Do not shadow LOCK's criticalblock variable for LOCK inside LOCK
* Do not shadow variables in src/wallet
* Berkeley DB v6 compatibility fix
* Reduce default number of blocks to check at startup
* Fix random segfault when closing Choose data directory dialog
* Fix several node initialization issues
* Add FEATURE_HD
* Improve handling of unconnecting headers
* Fix DoS vulnerability in mempool acceptance
* Bump default db cache to 300MiB
* Fix a bug where the SplashScreen will not be hidden during startup
* Stop trimming when mapTx is empty
* Evict orphans which are included or precluded by accepted blocks
* Reduce unnecessary hashing in signrawtransaction
* Watchdog check removed until Sentinel is updated/compatible fully
* Bump protocol versions to 70000
* Added IPv4 seed nodes to chainparamsseeds.h
* Update CHANGELOG


**Dynamic v1.1.0.0**

* Inline with BTC 0.12		
* HD Wallet Code Improvements		
* Remove/Replace Boost usage for c++11		
* Do not shadow member variables in httpserver		
* Update dbwrapper_tests.cpp		
* Access WorkQueue::running only within the cs lock		
* Use BIP32_HARDENED_KEY_LIMIT		
* Update NULL to use nullptr in GetWork & GetBlockTemplate		
* Few changes to governance rpc		
* Safety check in CInstantSend::SyncTransaction		
* Full path in 'failed to load cache' warnings		
* Refactor privateSendSigner		
* Net Fixes/DNS Seed Fix		
* Don't add non-current watchdogs to seen map		
* [RPC] remove the option of having multiple timer interfaces		
* Fix memory leak in httprpc.cpp		
* Make KEY_SIZE a compile-time constant
* Update CHANGELOG

** Initial Fork from Dash
