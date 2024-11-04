Name: Bereket Faltamo 

Instructions to run:
    1. docker build . -t prj4
    2. docker compose -f docker-compose-testcase-1.yml up (Replace the compose file)
    3. docker image prune && docker container prune && docker volume prune && docker system prune (docker pruning commands)