#include <RamCloud.h>
#include <string.h>
#include <OptionParser.h>

#define SIZE 10000000
#define VAL 42

using namespace RAMCloud;

int main(int argc, char** argv)
    try {
        Context context(false);

        OptionsDescription clientOptions("Client");
        OptionParser optionParser(clientOptions, argc, argv);

        std::string locator = optionParser.options.getCoordinatorLocator();
        RamCloud client(&context, locator.c_str(),
                optionParser.options.getClusterName().c_str());

        printf("Creating table..\n");
        client.createTable("test");

        uint64_t table;
        table = client.getTableId("test");

        printf("Writing in table..\n");

        char* key = "42";
        char* value = new char[SIZE];

        for (int i = 0; i < SIZE; ++i)
            value[i] = VAL;

        client.write(table, key, strlen(key), value, SIZE);

        Buffer buffer;
        client.read(table, key, strlen(key), &buffer);

        const char* bufferString = static_cast<const char*>(
                buffer.getRange(0, buffer.size()));

        assert(memcmp(bufferString, value, SIZE) == 0);

        printf("Exiting..\n");
        return 0;

    } catch (RAMCloud::ClientException& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        return 1;
    } catch (RAMCloud::Exception& e) {
        fprintf(stderr, "RAMCloud exception: %s\n", e.str().c_str());
        return 1;
    }

