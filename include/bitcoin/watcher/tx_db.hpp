/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-watcher.
 *
 * libbitcoin-watcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_WATCHER_TX_DB_HPP
#define LIBBITCOIN_WATCHER_TX_DB_HPP

#include <bitcoin/bitcoin.hpp>
#include <mutex>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <time.h>

namespace libwallet {

enum class tx_state
{
    /// The transaction has not been broadcast to the network.
    unsent,
    /// The network has seen this transaction, but not in a block.
    unconfirmed,
    /// The transaction is in a block.
    confirmed
};

typedef std::unordered_set<bc::payment_address> address_set;

/**
 * A list of transactions.
 *
 * This will eventually become a full database with queires mirroring what
 * is possible in the new libbitcoin-server protocol. For now, the goal is
 * to get something working.
 *
 * The fork-detection algorithm isn't perfect yet, since obelisk doesn't
 * provide the necessary information.
 */
class BC_API tx_db
{
public:
    BC_API ~tx_db();
    BC_API tx_db(unsigned unconfirmed_timeout=24*60*60);

    /**
     * Returns the highest block that this database has seen.
     */
    BC_API size_t last_height();

    /**
     * Returns true if the database contains a transaction.
     */
    BC_API bool has_tx(bc::hash_digest tx_hash);

    /**
     * Obtains a transaction from the database.
     */
    BC_API bc::transaction_type get_tx(bc::hash_digest tx_hash);

    /**
     * Finds a transaction's height, or 0 if it isn't in a block.
     */
    BC_API size_t get_tx_height(bc::hash_digest tx_hash);

    /**
     * Returns true if all inputs are addresses in the list control.
     */
    BC_API bool is_spend(bc::hash_digest tx_hash,
        const address_set& addresses);

    /**
     * Returns true if this address has received any funds.
     */
    BC_API bool has_history(const bc::payment_address& address);

    /**
     * Get all unspent outputs in the database.
     */
    BC_API bc::output_info_list get_utxos();

    /**
     * Get just the utxos corresponding to a set of addresses.
     */
    BC_API bc::output_info_list get_utxos(const address_set& addresses);

    /**
     * Write the database to an in-memory blob.
     */
    BC_API bc::data_chunk serialize();

    /**
     * Reconstitute the database from an in-memory blob.
     */
    BC_API bool load(const bc::data_chunk& data);

    /**
     * Debug dump to show db contents.
     */
    BC_API void dump(std::ostream& out);

    /**
     * Insert a new transaction into the database.
     * @return true if the callback should be fired.
     */
    BC_API bool insert(const bc::transaction_type &tx, tx_state state);

private:
    // - Updater: ----------------------
    friend class tx_updater;

    /**
     * Updates the block height.
     */
    void at_height(size_t height);

    /**
     * Mark a transaction as confirmed.
     * TODO: Require the block hash as well, once obelisk provides this.
     */
    void confirmed(bc::hash_digest tx_hash, size_t block_height);

    /**
     * Mark a transaction as unconfirmed.
     */
    void unconfirmed(bc::hash_digest tx_hash);

    /**
     * Delete a transaction.
     * This can happen when the network rejects a spend request.
     */
    BC_API void forget(bc::hash_digest tx_hash);

    /**
     * Call this each time the server reports that it sees a transaction.
     */
    BC_API void reset_timestamp(bc::hash_digest tx_hash);

    typedef std::function<void (bc::hash_digest tx_hash)> hash_fn;
    BC_API void foreach_unconfirmed(hash_fn&& f);
    BC_API void foreach_forked(hash_fn&& f);

    typedef std::function<void (const bc::transaction_type& tx)> tx_fn;
    BC_API void foreach_unsent(tx_fn&& f);

    // - Internal: ---------------------
    void check_fork(size_t height);

    // Guards access to object state:
    std::mutex mutex_;

    // The last block seen on the network:
    size_t last_height_;

    /**
     * A single row in the transaction database.
     */
    struct tx_row
    {
        // The transaction itself:
        bc::transaction_type tx;

        // State machine:
        tx_state state;
        size_t block_height;
        time_t timestamp;
        //bc::hash_digest block_hash; // TODO: Fix obelisk to return this

        // The transaction is certainly in a block, but there is some
        // question whether or not that block is on the main chain:
        bool need_check;
    };
    std::unordered_map<bc::hash_digest, tx_row> rows_;

    // Number of seconds an unconfirmed transaction must remain unseen
    // before we stop saving it:
    const unsigned unconfirmed_timeout_;
};

} // namespace libwallet

#endif

