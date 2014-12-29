#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <zmq.hpp>
#include <bitcoin/watcher.hpp>
#include "read_line.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

/**
 * A dynamically-allocated structure holding the resources needed for a
 * connection to a bitcoin server.
 */
class connection
{
public:
    connection(zmq::context_t& context,
        libwallet::tx_db& db, libwallet::tx_callbacks& cb)
      : socket_(context),
        codec_(socket_),
        updater_(db, codec_, cb)
    {
    }

    bc::client::zeromq_socket socket_;
    bc::client::obelisk_codec codec_;
    libwallet::tx_updater updater_;
};

/**
 * Command-line interface to the wallet watcher service.
 */
class cli
  : public libwallet::tx_callbacks
{
public:
    ~cli();
    cli();

    int run();

private:
    void command();
    void cmd_exit();
    void cmd_help();
    void cmd_connect(std::stringstream& args);
    void cmd_disconnect(std::stringstream& args);
    void cmd_watch(std::stringstream& args);
    void cmd_height();
    void cmd_tx_height(std::stringstream& args);
    void cmd_tx_dump(std::stringstream& args);
    void cmd_tx_send(std::stringstream& args);
    void cmd_utxos(std::stringstream& args);
    void cmd_save(std::stringstream& args);
    void cmd_load(std::stringstream& args);
    void cmd_dump(std::stringstream& args);

    // tx_callbacks interface:
    virtual void on_add(const bc::transaction_type& tx) override;
    virtual void on_height(size_t height) override;
    virtual void on_send(const std::error_code& error, const bc::transaction_type& tx) override;
    virtual void on_quiet() override;
    virtual void on_fail() override;

    bool check_connection();

    // Argument loading:
    bool read_string(std::stringstream& args, std::string& out,
        const std::string& error_message);
    bc::hash_digest read_txid(std::stringstream& args);
    bool read_address(std::stringstream& args, bc::payment_address& out);

    // Networking:
    zmq::context_t context_;
    read_line terminal_;
    connection *connection_;

    // State:
    libwallet::tx_db db_;
    bool done_;
};

cli::~cli()
{
    delete connection_;
}

cli::cli()
  : terminal_(context_),
    connection_(nullptr),
    done_(false)
{
}

/**
 * The main loop for the example application. This loop can be woken up
 * by either events from the network or by input from the terminal.
 */
int cli::run()
{
    std::cout << "type \"help\" for instructions" << std::endl;
    terminal_.show_prompt();

    while (!done_)
    {
        int delay = -1;
        std::vector<zmq_pollitem_t> items;
        items.reserve(2);
        items.push_back(terminal_.pollitem());
        if (connection_)
        {
            items.push_back(connection_->socket_.pollitem());
            auto next_wakeup = connection_->codec_.wakeup();
            if (next_wakeup.count())
                delay = next_wakeup.count();
        }
        zmq::poll(items.data(), items.size(), delay);

        if (items[0].revents)
            command();
        if (connection_ && items[1].revents)
            connection_->socket_.forward(connection_->codec_);
    }
    return 0;
}

/**
 * Reads a command from the terminal thread, and processes it appropriately.
 */
void cli::command()
{
    std::stringstream reader(terminal_.get_line());
    std::string command;
    reader >> command;

    if (command == "")                  ;
    else if (command == "exit")         cmd_exit();
    else if (command == "help")         cmd_help();
    else if (command == "connect")      cmd_connect(reader);
    else if (command == "disconnect")   cmd_disconnect(reader);
    else if (command == "height")       cmd_height();
    else if (command == "watch")        cmd_watch(reader);
    else if (command == "txheight")     cmd_tx_height(reader);
    else if (command == "txdump")       cmd_tx_dump(reader);
    else if (command == "txsend")       cmd_tx_send(reader);
    else if (command == "utxos")        cmd_utxos(reader);
    else if (command == "save")         cmd_save(reader);
    else if (command == "load")         cmd_load(reader);
    else if (command == "dump")         cmd_dump(reader);
    else
        std::cout << "unknown command " << command << std::endl;

    // Display another prompt, if needed:
    if (!done_)
        terminal_.show_prompt();
}

void cli::cmd_exit()
{
    done_ = true;
}

void cli::cmd_help()
{
    std::cout << "commands:" << std::endl;
    std::cout << "  exit              - leave the program" << std::endl;
    std::cout << "  help              - this menu" << std::endl;
    std::cout << "  connect <server>  - connect to obelisk server" << std::endl;
    std::cout << "  disconnect        - stop talking to the obelisk server" << std::endl;
    std::cout << "  height            - get the current blockchain height" << std::endl;
    std::cout << "  watch <address> [poll ms] - watch an address" << std::endl;
    std::cout << "  txheight <hash>   - get a transaction's height" << std::endl;
    std::cout << "  txdump <hash>     - show the contents of a transaction" << std::endl;
    std::cout << "  txsend <hash>     - push a transaction to the server" << std::endl;
    std::cout << "  utxos [address]   - get utxos for an address" << std::endl;
    std::cout << "  save <filename>   - dump the database to disk" << std::endl;
    std::cout << "  load <filename>   - load the database from disk" << std::endl;
    std::cout << "  dump [filename]   - display the database contents" << std::endl;
}

void cli::cmd_connect(std::stringstream& args)
{
    std::string server;
    if (!read_string(args, server, "error: no server given"))
        return;
    std::cout << "connecting to " << server << std::endl;

    delete connection_;
    connection_ = new connection(context_, db_, *this);
    if (!connection_->socket_.connect(server))
    {
        std::cout << "error: failed to connect" << std::endl;
        delete connection_;
        connection_ = nullptr;
        return;
    }
    connection_->updater_.start();
}

void cli::cmd_disconnect(std::stringstream& args)
{
    if (!check_connection())
        return;

    delete connection_;
    connection_ = nullptr;
}

void cli::cmd_height()
{
    std::cout << db_.last_height() << std::endl;
}

void cli::cmd_tx_height(std::stringstream& args)
{
    bc::hash_digest txid = read_txid(args);
    if (txid == bc::null_hash)
        return;
    int height;
    if (db_.has_tx(txid))
        std::cout << db_.get_tx_height(txid) << std::endl;
    else
        std::cout << "transaction not in database" << std::endl;
}

void cli::cmd_tx_dump(std::stringstream& args)
{
    bc::hash_digest txid = read_txid(args);
    if (txid == bc::null_hash)
        return;
    bc::transaction_type tx = db_.get_tx(txid);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.set_iterator(satoshi_save(tx, serial.iterator()));
    auto str = stream.str();
    std::cout << bc::encode_hex(str) << std::endl;
}

void cli::cmd_tx_send(std::stringstream& args)
{
    if (!check_connection())
        return;

    std::string arg;
    args >> arg;
    bc::data_chunk data = bc::decode_hex(arg);
    bc::transaction_type tx;
    try
    {
        bc::satoshi_load(data.begin(), data.end(), tx);
    }
    catch (bc::end_of_stream)
    {
        std::cout << "not a valid transaction" << std::endl;
        return;
    }
    connection_->updater_.send(tx);
}

void cli::cmd_watch(std::stringstream& args)
{
    if (!check_connection())
        return;

    bc::payment_address address;
    if (!read_address(args, address))
        return;
    unsigned poll_ms = 10000;
    args >> poll_ms;
    if (poll_ms < 500)
    {
        std::cout << "warning: poll too short, setting to 500ms" << std::endl;
        poll_ms = 500;
    }
    connection_->updater_.watch(address, bc::client::sleep_time(poll_ms));
}

void cli::cmd_utxos(std::stringstream& args)
{
    bc::output_info_list utxos;
    if (connection_)
        utxos = db_.get_utxos(connection_->updater_.watching());
    else
        utxos = db_.get_utxos();

    // Display the output:
    size_t total = 0;
    for (auto& utxo: utxos)
    {
        std::cout << bc::encode_hex(utxo.point.hash) << ":" <<
            utxo.point.index << std::endl;
        auto tx = db_.get_tx(utxo.point.hash);
        auto& output = tx.outputs[utxo.point.index];
        bc::payment_address to_address;
        if (bc::extract(to_address, output.script))
            std::cout << "address: " << to_address.encoded() << " ";
        std::cout << "value: " << output.value << std::endl;
        total += output.value;
    }
    std::cout << "total: " << total << std::endl;
}

void cli::cmd_save(std::stringstream& args)
{
    std::string filename;
    if (!read_string(args, filename, "no filename given"))
        return;

    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "cannot open " << filename << std::endl;
        return;
    }

    auto db = db_.serialize();
    file.write(reinterpret_cast<const char*>(db.data()), db.size());
    file.close();
}

void cli::cmd_load(std::stringstream& args)
{
    std::string filename;
    if (!read_string(args, filename, "no filename given"))
        return;

    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "cannot open " << filename << std::endl;
        return;
    }

    std::streampos size = file.tellg();
    uint8_t *data = new uint8_t[size];
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data), size);
    file.close();

    if (!db_.load(bc::data_chunk(data, data + size)))
        std::cerr << "error while loading data" << std::endl;
}

void cli::cmd_dump(std::stringstream& args)
{
    std::string filename;
    args >> filename;
    if (filename.size())
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "cannot open " << filename << std::endl;
            return;
        }
        db_.dump(file);
    }
    else
        db_.dump(std::cout);
}

void cli::on_add(const libbitcoin::transaction_type& tx)
{
    auto txid = libbitcoin::encode_hex(libbitcoin::hash_transaction(tx));
    std::cout << "got transaction " << txid << std::endl;
}

void cli::on_height(size_t height)
{
    std::cout << "got block " << height << std::endl;
}

void cli::on_send(const std::error_code& error, const bc::transaction_type& tx)
{
    if (error)
        std::cout << "failed to send transaction" << std::endl;
    else
        std::cout << "sent transaction" << std::endl;
}

void cli::on_quiet()
{
    std::cout << "query done" << std::endl;
    std::cout << "> " << std::flush;
}

void cli::on_fail()
{
    std::cout << "server error!" << std::endl;
}

/**
 * Verifies that a connection exists, and prints an error message otherwise.
 */
bool cli::check_connection()
{
    if (!connection_)
    {
        std::cout << "error: no connection" << std::endl;
        return false;
    }
    return true;
}

/**
 * Parses a string argument out of the command line,
 * or prints an error message if there is none.
 */
bool cli::read_string(std::stringstream& args, std::string& out,
    const std::string& error_message)
{
    args >> out;
    if (!out.size())
    {
        std::cout << error_message << std::endl;
        return false;
    }
    return true;
}

bc::hash_digest cli::read_txid(std::stringstream& args)
{
    std::string arg;
    args >> arg;
    if (!arg.size())
    {
        std::cout << "no txid given" << std::endl;
        return bc::null_hash;
    }
    return bc::decode_hash(arg);
}

/**
 * Reads a bitcoin address from the command-line, or prints an error if
 * the address is missing or invalid.
 */
bool cli::read_address(std::stringstream& args, bc::payment_address& out)
{
    std::string address;
    if (!read_string(args, address, "error: no address given"))
        return false;
    if (!out.set_encoded(address))
    {
        std::cout << "error: invalid address " << address << std::endl;
        return false;
    }
    return true;
}

int main()
{
    cli c;
    return c.run();
}
