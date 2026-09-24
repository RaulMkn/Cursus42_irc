NAME        := ircserv

CXX         := c++
CXXFLAGS    := -Wall -Wextra -Werror -std=c++98
INCFLAGS    := -Iinc

SRC_DIR     := src
OBJ_DIR     := obj
INC_DIR     := inc

SRCS        := $(wildcard $(SRC_DIR)/*.cpp)
OBJS        := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS        := $(OBJS:.o=.d)

GREEN       := \033[0;32m
CYAN        := \033[0;36m
RESET       := \033[0m

all: $(NAME)

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@printf "$(GREEN)[ircserv] binario listo: ./$(NAME)$(RESET)\n"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@printf "$(CYAN)[cc]$(RESET) %s\n" "$<"
	@$(CXX) $(CXXFLAGS) $(INCFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@printf "$(GREEN)[ircserv] objetos eliminados$(RESET)\n"

fclean: clean
	@rm -f $(NAME)
	@printf "$(GREEN)[ircserv] binario eliminado$(RESET)\n"

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
