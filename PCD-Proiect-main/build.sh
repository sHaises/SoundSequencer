g++-9 -o client client.cpp
g++-9 -o clientUNIX clientUNIX.cpp
g++-9 -o mixer mixer.cpp -lsfml-audio -lsndfile
g++-9 -o sequencer sequencer.cpp -lsfml-audio -lsndfile
g++-9 -o server server.cpp -pthread
g++-9 -o serverUNIX serverUNIX.cpp -pthread
g++-9 -o worker worker.cpp -pthread
echo build done
