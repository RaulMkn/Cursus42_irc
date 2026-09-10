# Contrato del objeto `Client`

Documento de referencia para trabajar en paralelo en `ft_irc`. Fija qué puede
tocar cada módulo del objeto compartido `Client`, en qué orden ocurren las cosas
dentro del `poll()` y cómo se libera todo sin dejar leaks ni punteros colgando.

- **Parte 1 — Red y core del servidor** (socket, `poll()`, `accept`, buffers).
- **Parte 2 — Protocolo IRC** (parseo, registro, canales, modos, privilegios).

Los dos módulos solo se ven a través de tres ficheros:

| Fichero | Dirección | Quién lo implementa |
|---|---|---|
| `inc/Client.hpp` | estado compartido | común (`src/Client.cpp`, ya hecho) |
| `inc/ICommandHandler.hpp` | Parte 1 → Parte 2 | Parte 2 (`CommandHandler`) |
| `inc/IServerContext.hpp` | Parte 2 → Parte 1 | Parte 1 (`Server`) |

Ninguna de las dos partes incluye headers de la otra. Esto permite compilar y
testear cada mitad por separado con un stub de la interfaz contraria.

---

## 1. Regla de oro

> La Parte 1 no interpreta ni un byte de IRC.
> La Parte 2 no llama a `recv`, `send`, `close`, `poll` ni toca un `fd`.

Todo el tráfico entre capas pasa por dos buffers dentro de `Client`:

```
socket --recv()--> _readBuffer --extractLine()--> execute(client, line)
                                                          |
socket <--send()-- _writeBuffer <---- enqueue(respuesta) <-+
```

La Parte 2 nunca escribe "ahora mismo": deja el texto en `_writeBuffer` y sigue.
El siguiente ciclo de `poll()` se encarga de sacarlo por el socket. Esto es lo
que mantiene el servidor no bloqueante.

## 2. Propiedad de cada campo

| Campo | Escribe | Lee |
|---|---|---|
| `_fd`, `_host` | constructor (Parte 1) | ambas |
| `_readBuffer` | Parte 1 | Parte 1 |
| `_writeBuffer` | ambas (`enqueue` / `consumeWriteBuffer`) | Parte 1 |
| `_state` | Parte 2 | ambas |
| `_nick`, `_user`, `_realname` | Parte 2 | ambas |
| `_channels` | Parte 2 | ambas |
| `_markedForClose`, `_quitReason` | ambas (`markForClose`) | Parte 1 |

Nadie escribe en un campo que no le corresponde, ni siquiera "solo por
comodidad". Si hace falta un dato nuevo, se añade al header y se anota en esta
tabla antes de usarlo.

## 3. Ciclo de vida de un cliente

1. **Alta.** `POLLIN` en el socket de escucha → `accept()` → `fcntl` no
   bloqueante → `new Client(fd, host)` → se guarda en el mapa `fd -> Client*` de
   la Parte 1. El estado inicial es `HANDSHAKE`.
2. **Lectura.** `POLLIN` en el fd del cliente → `recv()` a un buffer local →
   `appendToReadBuffer()`. Si devuelve `false` la línea pendiente pasó de 512
   bytes sin `CRLF`: se descarta el buffer y se llama a `markForClose()`.
3. **Despacho.** Bucle `while (client.extractLine(line))` →
   `handler.execute(client, line)` por cada comando completo. Un solo `recv()`
   puede producir cero, uno o varios comandos. Nunca se asume que un paquete es
   un comando.
4. **Escritura.** Se registra `POLLOUT` **solo** si `hasPendingOutput()` es
   `true`. Al llegar el evento: `send(fd, peekWriteBuffer().c_str(), size, 0)` y
   `consumeWriteBuffer(n)` con los bytes realmente enviados. `send` puede enviar
   menos de lo pedido, por eso el consumo es parcial y no un `clear()`.
5. **Baja.** `recv() == 0`, error de socket o `isMarkedForClose()`. Orden
   obligatorio:
   1. si está marcado, intentar vaciar `_writeBuffer` (para que el `ERROR :...`
      final llegue),
   2. `handler.onClientDisconnect(client)`,
   3. borrar la entrada del mapa y del array de `pollfd`,
   4. `close(fd)`,
   5. `delete` del objeto.

### El bug que hay que evitar

Los canales de la Parte 2 guardan `Client*`. Si la Parte 1 hace `delete` sin
llamar antes a `onClientDisconnect()`, esos punteros quedan colgando y el
servidor casca en el siguiente `PRIVMSG` al canal. El paso 5.2 no es opcional.

Al revés también: la Parte 2 no guarda `Client*` más allá de sus estructuras de
canal, y los punteros que devuelve `IServerContext::findClientByNick()` son
prestados. Se usan y se olvidan. Nunca se hace `delete` sobre ellos.

## 4. Máquina de estados del registro

```
HANDSHAKE --PASS correcto--> PASS_OK --NICK + USER--> REGISTERED
```

- En `HANDSHAKE` y `PASS_OK` solo se aceptan `PASS`, `NICK`, `USER`, `CAP` y
  `QUIT`. Cualquier otro comando responde `451 ERR_NOTREGISTERED`.
- `PASS` incorrecto: `464 ERR_PASSWDMISMATCH` + `markForClose("Bad password")`.
- La transición a `REGISTERED` la hace la Parte 2 cuando ya tiene nick y user;
  ahí envía el bloque de bienvenida `001`–`004`.
- El orden real de los clientes suele ser `CAP LS`, `PASS`, `NICK`, `USER`.
  `CAP` se puede contestar con `CAP * LS :` para cerrar la negociación.

## 5. Detalles de los buffers

**Entrada.** `extractLine()` corta por `\n`, quita el `\r` previo si existe y
descarta líneas vacías, que el RFC obliga a ignorar. Por eso acepta tanto
clientes reales (`\r\n`) como `nc` (`\n` suelto) sin código extra en la Parte 2.
La línea que llega a `execute()` está garantizada no vacía y sin terminador.

**Salida.** `enqueue()` añade el `CRLF` si el mensaje no lo trae, así que la
Parte 2 puede escribir `client.enqueue(":servidor 001 bob :Welcome")` sin
acordarse del terminador. Es idempotente: no duplica el `CRLF`.

**Broadcast.** Para retransmitir a un canal, la Parte 2 recorre sus miembros y
llama a `enqueue()` en cada uno, incluido el remitente si el comando lo exige
(`JOIN`, `PART`) o excluyéndolo (`PRIVMSG`). No hace falta avisar a la red: en
el siguiente ciclo detecta el `POLLOUT` pendiente de esos fds.

## 6. Recursos y leaks

- Un `new` por cliente en `accept()`, un `delete` en la baja. El destructor del
  `Server` recorre el mapa y borra los que queden vivos al apagar.
- Todo `fd` abierto (escucha incluido) se cierra en el destructor del `Server`.
- Los `std::string` de los buffers se liberan solos; lo único que hay que
  vigilar es que crezcan sin límite, de ahí el corte de 512 bytes en entrada.
- Comprobación: `valgrind --leak-check=full ./ircserv 6667 pass` en Linux, o
  `leaks --atExit -- ./ircserv 6667 pass` en macOS.

## 7. Esqueleto de cada lado

Parte 1, corazón del bucle (recortado):

```cpp
if (revents & POLLIN)
{
    char    buf[1024];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);

    if (n <= 0)
        return (disconnect(client));
    if (!client->appendToReadBuffer(buf, static_cast<std::size_t>(n)))
        client->markForClose("Input line too long");

    std::string line;
    while (client->extractLine(line))
        _handler->execute(*client, line);
}
```

Parte 2, forma de un comando:

```cpp
void    CommandHandler::cmdJoin(Client &client, const std::vector<std::string> &args)
{
    if (!client.isRegistered())
        return (client.enqueue(numeric(client, 451, ":You have not registered")));
    if (args.empty())
        return (client.enqueue(numeric(client, 461, "JOIN :Not enough parameters")));
    // ... alta en el canal ...
    client.addChannel(name);
    channel->broadcast(client.prefix() + " JOIN " + name);
}
```

## 8. Cómo trabajar en paralelo desde ya

La Parte 2 no necesita esperar a que la red funcione. Con un `main` de prueba se
puede ejercitar toda la lógica sin sockets:

```cpp
Client  fake(-1, "localhost");

fake.appendToReadBuffer("PASS hunter2\r\nNICK bob\r\n", 24);
while (fake.extractLine(line))
    handler.execute(fake, line);
std::cout << fake.peekWriteBuffer();   // aqui se leen las respuestas
```

El `fd = -1` es intencionado: si algún día ese objeto acaba en un `send()`, falla
de forma escandalosa en vez de silenciosa.
