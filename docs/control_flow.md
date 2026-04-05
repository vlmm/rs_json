
Ето обобщена **Mermaid** диаграма на логиката на `RS_JSON`: сериен прием, проверка на контролна сума, и разклоняване по режим (MASTER/SLAVE) и команда.

```mermaid
flowchart TB
    subgraph TX["Изпращане (sendMessage / ping / discover / requestData / sendAck)"]
        A1[Построяване на JSON: address, from, command, data]
        A2[Сериализация в низ]
        A3[checksum = сума от байтовете на JSON mod 256 → 2 hex символа]
        A4[rawTransmit: опционално DE HIGH → print съобщение + newline → DE LOW]
        A1 --> A2 --> A3 --> A4
    end

    subgraph RX["Прием (listen)"]
        B1{Има байт в serial?}
        B2[Добави към buffer]
        B3[processMessage buffer]
        B4[Изчисти buffer]
        B1 -->|да| B5{символ \\n или \\r?}
        B5 -->|не| B2 --> B1
        B5 -->|да| B3 --> B4 --> B1
    end

    subgraph PM["processMessage"]
        C1{дължина ≥ 3?}
        C2[jsonPart = без последните 2 символа<br/>checksumPart = последните 2]
        C3{checksum валиден?}
        C4[deserializeJson → address, from, command, data]
        C5{режим?}
        C1 -->|не| Z[изход]
        C1 -->|да| C2 --> C3
        C3 -->|не| Z
        C3 -->|да| C4 --> C5
    end

    B3 --> C1

    subgraph SLAVE["SLAVE"]
        S0{address == моя ИЛИ broadcast?}
        S1{command == request?}
        S2{lastSentId == lastAckedId?}
        S3[lastSentId++<br/>нов JSON data + id<br/>запази lastSentPayload<br/>rawTransmit]
        S4[rawTransmit lastSentPayload<br/>повторно изпращане]
        S5{command == ack?}
        S6{data.id == lastSentId?}
        S7[lastAckedId = id]
        S8[callback jsonPart]
        S0 -->|не| Z
        S0 -->|да| S1
        S1 -->|да| S2
        S2 -->|да| S3
        S2 -->|не| S4
        S1 -->|не| S5
        S5 -->|да| S6
        S6 -->|да| S7
        S5 -->|не| S8
    end

    subgraph MASTER["MASTER"]
        M1{command == data?}
        M2{receivedAddress == моя?}
        M3{id != lastProcessedId?}
        M4[lastProcessedId = id<br/>callback jsonPart]
        M5[sendAck senderAddress, id<br/>винаги, вкл. дубликати]
        M6{ack или request?}
        M7{други команди<br/>и receivedAddress != моя?}
        M8[callback jsonPart]
        M1 -->|да| M2
        M2 -->|не| Z
        M2 -->|да| M3
        M3 -->|да| M4
        M3 -->|не| M5
        M4 --> M5
        M1 -->|не| M6
        M6 -->|да| Z
        M6 -->|не| M7
        M7 -->|да| M8
    end

    C5 -->|SLAVE| S0
    C5 -->|MASTER| M1
```

**Кратко обяснение:** изходящите съобщения винаги са JSON + двубайтова hex контролна сума и нов ред; при вход `listen` събира до край на реда, после `processMessage` отрязва сумата, валидира я и по режим обработва `request`/`ack`/`data` и останалите команди. На SLAVE при непотвърдено съобщение при нов `request` се праща същият `lastSentPayload`; на MASTER при `data` се дедуплицира по `id`, но **ack се изпраща винаги**, за да може слейвът да напредне с `lastAckedId`.

Ако искаш, мога да добавя отделна мини-диаграма само за последователността MASTER `requestData` ↔ SLAVE `data` ↔ MASTER `ack`.