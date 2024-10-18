Name: Bereket Faltamo 

Instructions to run:
    1. docker build . -t prj3
    2. docker compose -f docker-compose-testcase-1.yml up
    3. docker image prune && docker container prune && docker volume prune && docker system prune