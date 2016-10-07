#include <cstdlib>
#include <ios>
#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <DrbdResource.h>
#include <StringTokenizer.h>
#include <QTree.h>

#include "comparators.h"
#include "LiveStatus.h"
#include "EventsSourceSpawner.h"
#include "EventsIo.h"

const int BUFSZ = 1000;

int main(int argc, char* argv[])
{
    int rc = 0;
    EventsSourceSpawner* evsp {nullptr};
    EventsIo* evio {nullptr};
    char* buffer {nullptr};
    try
    {
        buffer = new char[BUFSZ];
        evsp = new EventsSourceSpawner();
        int evfd = evsp->spawn_source();
        std::cerr << "Source spawned, pid: " << static_cast<unsigned long long> (evsp->get_process_id()) <<
                  " fd: " << evfd << std::endl;
        if (evfd != -1)
        {
            evio = new EventsIo(evfd);
            bool shutdown = false;
            while (!shutdown)
            {
                EventsIo::event event_id = evio->wait_event();
                switch (event_id)
                {
                    case EventsIo::event::EVENT_LINE:
                        {
                            std::string* event_line = evio->get_event_line();
                            if (event_line != nullptr)
                            {
                                std::cout << "EVENT_LINE ready (" << *event_line << ")" << std::endl;
                            }
                            else
                            {
                                std::cerr << "EVENT_LINE ready, but get_event_line() == nullptr" << std::endl;
                            }
                        }
                        break;
                    case EventsIo::event::SIGNAL:
                        {
                            int signal_id = evio->get_signal();
                            switch (signal_id)
                            {
                                case SIGHUP:
                                    // fall-through
                                case SIGINT:
                                    // fall-through
                                case SIGTERM:
                                    std::cerr << "Received terminate notification" << std::endl;
                                    shutdown = true;
                                    break;
                                case SIGWINCH:
                                    std::cerr << "Received window size change notification" << std::endl;
                                    break;
                                default:
                                    std::cerr << "Received unexpected signal #" << signal_id << std::endl;
                                    break;
                            }
                        }
                        break;
                    case EventsIo::event::STDIN:
                        {
                            char c = std::fgetc(stdin);
                            std::fprintf(stdout, "STDIN ready (%c)\n", c);
                        }
                        break;
                    case EventsIo::event::NONE:
                        // fall-through, not supposed to happen
                    default:
                        // ignore any other events
                        break;
                }
            }
        }
    }
    catch (std::bad_alloc& out_of_memory_exc)
    {
        std::cerr << "Out of memory\n";
        rc = 1;
    }
    catch (EventsSourceException&)
    {
        std::cerr << "caught EventsSourceException" << std::endl;
    }
    catch (EventsIoException&)
    {
        std::cerr << "caught EventsIoException" << std::endl;
    }

    delete evio;
    delete evsp;
    delete[] buffer;

    std::cout << "test: Finished, rc=" << rc << std::endl;

    return rc;
}
