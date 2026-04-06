### Контролен поток за режим **MASTER**:

Тази диаграма на контролния поток показва основните действия, които се извършват в модула `RS_JSON`. Тя обхваща основните функции като инициализация, изпращане на съобщения, получаване и обработка на съобщения, валидиране на контролни суми и обработка на събития:

```mermaid
flowchart TD
    A[Start] --> B[RS_JSON Constructor]
    B --> C{Mode == MASTER?}
    C -->|Yes| D[Skip message if address is for this device]
    C -->|No| E[Process message if address matches or is broadcast]
    D --> F[Send message to serial]
    E --> F
    F --> G[Calculate checksum]
    G --> H[Append checksum to message]
    H --> I[Send message via serial]
    I --> J[Listen for incoming messages]
    J --> K{Message available?}
    K -->|Yes| L[Process message]
    K -->|No| J
    L --> M[Extract address, command, and data from JSON]
    M --> N{Checksum valid?}
    N -->|No| O[Discard message]
    N -->|Yes| P[Handle command or callback]
    O --> J
    P --> J
    F --> Q[Wait for next message]
    Q --> J
    subgraph Ping and Discover
        P1[Ping] --> P2[Send ping to address]
        P3[Discover] --> P4[Send discover message]
    end
    A --> B
```

### Обяснение на контролната диаграма:

1. **Конструктори**: Има два конструктора за инициализация на обекта `RS_JSON`. В зависимост от режима на работа и дали е зададен `dePin`, ще се използва различен конструктор.

2. **Основен цикъл**: В метода `listen` се проверява дали има нови съобщения в серийния порт. Ако има, те се обработват и се валидират.

3. **Валидация на сума**: При всяко получаване на съобщение се изчислява контролна сума и се сравнява с последните два символа от съобщението.

4. **Изпращане на съобщения**: Когато е необходимо да се изпрати съобщение, се създава JSON обект, добавя се контролна сума и съобщението се изпраща през серийния порт. В режим MASTER се изпращат съобщения без да се обработват тези, адресирани до устройството.

---

### Контролен поток за режим **SLAVE**:

```mermaid
flowchart TD
    A[Start] --> B[RS_JSON Constructor]
    B --> C{Mode == SLAVE?}
    C -->|Yes| D[Listen for incoming messages]
    C -->|No| F[Skip (MASTER mode)]
    D --> G{Message available?}
    G -->|Yes| H[Process message]
    G -->|No| D
    H --> I[Extract address, command, and data from JSON]
    I --> J{Checksum valid?}
    J -->|No| K[Discard message]
    J -->|Yes| L[Handle command or callback]
    K --> D
    L --> D
    F --> Q[Wait for next message]
    Q --> D
    subgraph Ping and Discover
        P1[Ping] --> P2[Send ping to address]
        P3[Discover] --> P4[Send discover message]
    end
    A --> B
```

### Обяснение на контролния поток в **SLAVE** режим:

1. **Конструктор**: Модулът се инициализира чрез конструктора, който задава режим и други параметри. В режим **SLAVE** модула ще слуша за входящи съобщения, които са адресирани до него или са broadcast съобщения.

2. **Слушане за съобщения**: Основната задача на **SLAVE** устройството е да слуша за съобщения в серийния порт чрез метода `listen`. Когато се появи ново съобщение, то се обработва.

3. **Обработка на съобщения**: След като съобщението е получено, то се обработва, като се извличат адреса, командата и данните от JSON-а.

4. **Валидация на контролна сума**: Проверява се дали контролната сума на съобщението съвпада с тази, която е изчислена от самото съобщение.

5. **Извършване на действия в зависимост от командата**: Ако сумата е валидна, се обработва командата и се извиква callback функцията, ако такава е зададена и условията за извикването са изпълнени (например адресът на съобщението съвпада с адреса на устройството или е broadcast съобщение).

6. **Ping и Discover**: Модулът може да изпрати **ping** съобщения или да поиска открития на устройства в мрежата.

Това обхваща основните стъпки за обработка на съобщения и тяхната валидация при режим **SLAVE**.


6. **Ping и Discover**: Методите `ping` и `discoverDevices` изпращат специфични съобщения за откриване на устройства или пинг.

Тази диаграма обхваща всички основни действия, като изпращане и получаване на съобщения, валидация на данни и обработка на команди.
