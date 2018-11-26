/* состояние программы, T13.788-T13.841; $DVS:time$ */

dag_state(INIT, "Initializing.")
dag_state(KEYS, "Generating keys...")
dag_state(REST, "The local storage is corrupted. Resetting blocks engine.")
dag_state(LOAD, "Loading blocks from the local storage.")
dag_state(STOP, "Blocks loaded. Waiting for 'run' command.")
dag_state(WTST, "Trying to connect to the test network.")
dag_state(WAIT, "Trying to connect to the main network.")
dag_state(TTST, "Trying to connect to the testnet pool.")
dag_state(TRYP, "Trying to connect to the mainnet pool.")
dag_state(CTST, "Connected to the test network. Synchronizing.")
dag_state(CONN, "Connected to the main network. Synchronizing.")
dag_state(XFER, "Waiting for transfer to complete.")
dag_state(PTST, "Connected to the testnet pool. No mining.")
dag_state(POOL, "Connected to the mainnet pool. No mining.")
dag_state(MTST, "Connected to the testnet pool. Mining on. Normal testing.")
dag_state(MINE, "Connected to the mainnet pool. Mining on. Normal operation.")
dag_state(STST, "Synchronized with the test network. Normal testing.")
dag_state(SYNC, "Synchronized with the main network. Normal operation.")
