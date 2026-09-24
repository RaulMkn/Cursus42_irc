# Guía de evaluación — ft_irc

Guía para defender el proyecto en la evaluación entre pares de 42. Cubre
compilación, ejecución, pruebas y las preguntas típicas del corrector.

---

## 0. Antes de empezar

```bash
make re          # recompila desde cero
./ircserv 6667 hunter2
```

Comprueba que:
- Compila con `c++ -Wall -Wextra -Werror -std=c++98` sin warnings.
- `make` no re-linka si no hay cambios (ejecuta `make` dos veces: la segunda
  no debe recompilar nada).

---

## 1. Comprobaciones que dan 0 si fallan (verifícalas primero)

| Requisito | Cómo demostrarlo |
|---|---|
| Un solo `poll()` (o equivalente) | `grep -n poll src/Server.cpp` → un único bucle |
| Nunca recv/send sin poll | Todo recv/send está en `handleRead`/`handleWrite`, invocados desde el bucle de poll |
| No usar `errno` tras recv/send | El único `errno` es `EINTR` tras `poll()` (permitido) |
| fcntl solo con O_NONBLOCK | `grep -n fcntl src/Server.cpp` → `fcntl(fd, F_SETFL, O_NONBLOCK)` |
| No crashea nunca | Ver sección 4 (desconexiones abruptas, SIGPIPE) |
| C++98 | flag `-std=c++98` en el Makefile |

---

## 2. Argumentos y arranque

```bash
./ircserv                 # error de uso, exit 1
./ircserv 6667            # faltan args, exit 1
./ircserv abc pass        # puerto no numérico, exit 1
./ircserv 70000 pass      # puerto fuera de rango, exit 1
./ircserv 6667 ""         # password vacía, exit 1
./ircserv 6667 hunter2    # arranca
```

---

## 3. Prueba funcional con netcat

Terminal servidor:
```bash
./ircserv 6667 hunter2
```

Terminal cliente:
```bash
nc -C 127.0.0.1 6667
PASS hunter2
NICK bob
USER bob 0 * :Bob
JOIN #42madrid
PRIVMSG #42madrid :hola
```

Se debe recibir el bloque de bienvenida `001`–`004`, el `JOIN`, el `RPL_NAMREPLY`
(353) y `RPL_ENDOFNAMES` (366).

### Prueba de paquetes fragmentados (obligatoria en el subject)

Con `nc`, escribe una parte del comando, pulsa **Ctrl+D**, escribe otra parte,
Ctrl+D, y termina con el resto + Enter:

```
PRI      (Ctrl+D)
VMSG #42madrid    (Ctrl+D)
 :mensaje partido    (Enter)
```

El servidor debe reconstruir el comando completo y entregarlo una sola vez.

---

## 4. Prueba de robustez (no debe crashear)

1. **Desconexión abrupta**: conecta dos clientes al mismo canal, mata uno con
   Ctrl+C. El otro debe recibir el `QUIT` y el servidor sigue vivo.
2. **PRIVMSG a nick inexistente** → `401 ERR_NOSUCHNICK`, sin crash.
3. **Comando antes de registrarse** (p. ej. `JOIN` sin PASS/NICK/USER) →
   `451 ERR_NOTREGISTERED`.
4. **Password incorrecto** → `464 ERR_PASSWDMISMATCH` y cierre de conexión.
5. **Ctrl+C en el servidor** → salida ordenada (cierra sockets, libera memoria).

---

## 5. Prueba con cliente de referencia (irssi)

El corrector usará un cliente real. Pruébalo tú antes:

```bash
irssi
/connect 127.0.0.1 6667 hunter2
/join #test
/msg #test hola
/topic #test :bienvenida
/mode #test +t
/whois bob        # opcional, puede no estar implementado
```

Dos clientes irssi conectados al mismo canal deben verse los mensajes.

---

## 6. Comandos implementados (para enseñar al evaluador)

- **Registro**: `PASS`, `NICK`, `USER`, `CAP`, `PING`, `QUIT`
- **Canales**: `JOIN`, `PART`, `PRIVMSG`, `NOTICE`
- **Operador**: `TOPIC`, `KICK`, `INVITE`, `MODE`
- **MODE**: `+i` (invite-only), `+t` (topic restringido), `+k` (clave),
  `+o` (operador), `+l` (límite de usuarios)

### Escenario completo de modos para la defensa

```
# cliente A (operador porque crea el canal)
JOIN #room
MODE #room +i               # invite-only
INVITE bob #room            # invitar a B
MODE #room +k secreto       # poner clave
MODE #room +l 5             # límite 5
MODE #room +o bob           # dar op a B
TOPIC #room :sala de pruebas
KICK #room bob :fuera
```

Verifica que un usuario NO operador recibe `482 ERR_CHANOPRIVSNEEDED` al intentar
cualquiera de estos.

---

## 7. Comprobación de fugas de memoria

**Linux:**
```bash
valgrind --leak-check=full ./ircserv 6667 pass
```

**macOS:**
```bash
./ircserv 6667 pass &
leaks <pid>        # 0 leaks for 0 total leaked bytes
```

---

## 8. Preguntas típicas del evaluador y respuestas

**¿Por qué un solo `poll()`?**
Para ser no bloqueante y single-threaded: un solo hilo atiende todos los
clientes reaccionando a los eventos que devuelve `poll`, sin quedarse esperando
en ningún recv/send.

**¿Cómo manejas datos parciales?**
`recv` deja los bytes en `_readBuffer` del cliente (`appendToReadBuffer`).
`extractLine` va sacando comandos completos cortando por `\n` y quitando el `\r`.
Un `recv` puede producir 0, 1 o varios comandos.

**¿Cómo evitas bloquear en `send`?**
Solo se registra `POLLOUT` cuando hay salida pendiente. `send` puede enviar
menos de lo pedido, por eso `consumeWriteBuffer` consume solo los bytes
realmente enviados y el resto se manda en el siguiente ciclo.

**¿Por qué separas red (Parte 1) y protocolo (Parte 2)?**
Se comunican solo por tres interfaces (`Client`, `ICommandHandler`,
`IServerContext`). La red no interpreta IRC y el protocolo no toca sockets. Eso
permite testear cada mitad por separado.

**¿Cómo evitas punteros colgando al desconectar?**
La red llama a `onClientDisconnect()` ANTES de `delete`. Ahí el protocolo saca
al cliente de todos sus canales, así los `Channel` no guardan punteros muertos.

**¿Por qué ignoras SIGPIPE?**
Si un cliente cierra y el servidor hace `send`, el SO envía SIGPIPE que mataría
el proceso. Ignorándolo, `send` devuelve error y se gestiona la desconexión.

---

## 9. Puntos honestos a tener en cuenta

- Probado con script propio (protocolo IRC) + comprobaciones de leaks. **Falta
  validarlo con irssi/nc en tu máquina** antes de la defensa: el evaluador usará
  un cliente real.
- `MODE #canal` sin flags no muestra los modos actuales (no es obligatorio).
- `NICK` en colisión es case-insensitive (cumple RFC).
