### Контролен поток за режим **MASTER**:

Тази диаграма на контролния поток показва основните действия, които се извършват в модула `RS_JSON`. Тя обхваща основните функции като инициализация, изпращане на съобщения, получаване и обработка на съобщения, валидиране на контролни суми и обработка на събития:

```mermaid
flowchart TD
    A[Start] --> B[RS_JSON Constructor]
    B --> R[setCallback<callback>]
    R --> C{Mode}
    C -->|MASTER| G[Master - Listen for incoming response]
    
    D[Master - Send Message Request]
    C -->|SLAVE| E[Slave FSM]
    
    D --> H[Construct Message with checksum]
    H --> I[Send message via serial]
    I --> G

    G --> J{Message received?}
    J -->|No| G
    J -->|Yes| K[Process Message <Checksum validation>]
    K --> L{Checksum Valid?}
    L -->|No| M[Discard message and print error]
    L -->|Yes| N[Parse JSON message]
    M --> G
    
    N --> O{Is address != own address?}
    O -->|Yes| P[Trigger callback with received message]
    O -->|No| Q[Ignore message]
    P --> G
    Q --> G
```
### Какво е показва диаграмата:

1. **RS_JSON Constructor**: Тук започва процесът с инициализацията на обекта. След това се задава **callback**.

2. **Избор на режим (MASTER или SLAVE)**:

   * **MASTER** режимът започва с **слушане за отговори** (`Master - Listen for incoming response`).
   * **SLAVE** режимът преминава в състояние за обработка на съобщения чрез **Slave FSM**, което е подходящо за логиката на устройството в този режим.

3. **MASTER режим**:

   * Ако **MASTER** трябва да изпрати съобщение, започва с **изпращане на съобщение** (`Master - Send Message Request`), след което съобщението се изгражда и изпраща със съответната контролна сума.
   * След изпращането, **MASTER** отново влиза в **слушане за отговори** (`Listen for incoming response`).

4. **Обработка на съобщение в MASTER режим**:

   * Когато **MASTER** получи съобщение, първо се проверява дали е валидно чрез **контролна сума**.
   * Ако контролната сума не съвпада, съобщението се изхвърля с **грешка**.
   * Ако е валидно, съобщението се парсира и проверява дали е адресирано към устройството.
     * Ако **адресът съвпада** със собствения, се извиква **callback**.
     * Ако **адресът не съвпада**, съобщението се игнорира.
---

### Контролен поток за режим **SLAVE**:

```mermaid
flowchart TD
    A[Start] --> B[RS_JSON Constructor]
    B --> C[setCallback<callback>]
    C --> D{Mode}
    D -->|SLAVE| E[Slave - Listen for incoming message]
    
    E --> F{Message received?}
    F -->|No| E
    F -->|Yes| G[Process Message <Checksum validation>]
    G --> H{Checksum Valid?}
    H -->|No| I[Discard message and print error]
    H -->|Yes| J[Parse JSON message]
    I --> E
    
    J --> K{Is address == own address or broadcast?}
    K -->|Yes| L[Trigger callback with received message]
    K -->|No| M[Ignore message]
    L --> E
    M --> E
```

### Обяснение на стъпките:

1. **Слушане за входящо съобщение** (`Slave - Listen for incoming message`):

   * SLAVE режимът постоянно слуша за входящи съобщения от **MASTER** или други устройства.

2. **Получаване на съобщение**:

   * Когато съобщение е получено, то се проверява за валидност чрез **контролна сума**.

3. **Проверка на контролна сума**:

   * Ако контролата на съобщението не е валидна, съобщението се изхвърля и се отпечатва грешка.

4. **Парсване на съобщението**:

   * Ако контролната сума е валидна, съобщението се парсира в JSON формат.

5. **Адресиране на съобщението**:

   * Ако съобщението е адресирано към SLAVE устройството (или към "broadcast"), извиква се **callback** функцията, която обработва съобщението.
   * Ако съобщението не е адресирано към SLAVE устройството, то се игнорира.

6. **Цикличност**:

   * След обработката на съобщението, **SLAVE** отново преминава в състояние на слушане за ново съобщение.
