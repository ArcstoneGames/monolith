# Realistic Room Sizes and Floor-by-Floor Layout Rules for Procedural Buildings

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Affects:** `MonolithMeshFloorPlanGenerator`, Building Archetype JSONs

## Table of Contents

1. [Grid Cell Math](#grid-cell-math)
2. [Current Archetype Problems](#current-archetype-problems)
3. [Building Type 1: Residential House](#1-residential-house)
4. [Building Type 2: Small Clinic / Medical Office](#2-small-clinic--medical-office)
5. [Building Type 3: Office Building (Multi-Floor)](#3-office-building-multi-floor)
6. [Building Type 4: Police Station](#4-police-station)
7. [Building Type 5: Apartment Building (Multi-Unit)](#5-apartment-building-multi-unit)
8. [Building Type 6: Retail / Commercial](#6-retail--commercial)
9. [Building Type 7: Warehouse / Industrial](#7-warehouse--industrial)
10. [Building Type 8: Church / Chapel](#8-church--chapel)
11. [Building Type 9: School](#9-school)
12. [Universal Building Code Rules](#universal-building-code-rules)
13. [Universal Adjacency Rules](#universal-adjacency-rules)
14. [Archetype Format Extensions Needed](#archetype-format-extensions-needed)
15. [Sources](#sources)

---

## Grid Cell Math

- **Grid cell size:** 50cm x 50cm = 0.25 m² per cell
- **Conversion:** `grid_cells = real_world_m² * 4`
- **Reverse:** `real_world_m² = grid_cells * 0.25`
- **Linear:** 1m = 2 cells, 3m = 6 cells, 5m = 10 cells

**Quick reference table:**

| Real-World m² | Grid Cells | Example Room |
|---------------|-----------|--------------|
| 2.5 m² | 10 | Half bath |
| 5.0 m² | 20 | Full bathroom |
| 9.0 m² | 36 | Small bedroom |
| 12.0 m² | 48 | Bedroom |
| 15.0 m² | 60 | Kitchen |
| 20.0 m² | 80 | Living room |
| 25.0 m² | 100 | Master bedroom |
| 30.0 m² | 120 | Large living room |
| 50.0 m² | 200 | Open office |
| 100.0 m² | 400 | Warehouse floor |

---

## Current Archetype Problems

The existing archetype JSONs use `min_area` / `max_area` values that are **in grid cells** but the numbers are far too small for real rooms:

| Room | Current min_area (cells) | Current m² | Real-World m² | Should Be (cells) |
|------|------------------------|-----------|--------------|-------------------|
| Living room (house) | 20 | 5.0 | 18-27 | 72-108 |
| Kitchen (house) | 12 | 3.0 | 10-18 | 40-72 |
| Bedroom (house) | 12 | 3.0 | 10-15 | 40-60 |
| Bathroom (house) | 6 | 1.5 | 4-6 | 16-24 |
| Entryway (house) | 4 | 1.0 | 3-6 | 12-24 |
| Bullpen (police) | 30 | 7.5 | 40-80 | 160-320 |
| Holding cell (police) | 4 | 1.0 | 6-8 | 24-32 |

**Everything is roughly 4-5x too small.** The current values appear to be treating cells as if each were 1 m² when they're actually 0.25 m².

---

## 1. Residential House

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio |
|------|-------------------|-----------------|---------------------|--------------|
| Living room | 4.0x5.0 to 5.5x7.0 | 18-35 | 72-140 | 1.0-1.5:1 |
| Kitchen (galley) | 2.5x4.0 to 3.0x5.0 | 10-15 | 40-60 | 1.5-2.0:1 |
| Kitchen (open plan) | 3.5x4.5 to 4.5x6.0 | 15-25 | 60-100 | 1.0-1.5:1 |
| Master bedroom | 4.0x4.5 to 5.0x6.0 | 18-28 | 72-112 | 1.0-1.3:1 |
| Secondary bedroom | 3.0x3.5 to 4.0x4.5 | 10-18 | 40-72 | 1.0-1.3:1 |
| Full bathroom | 2.0x2.5 to 2.5x3.5 | 5-8 | 20-32 | 1.0-1.4:1 |
| Half bath / powder room | 1.5x1.5 to 1.5x2.5 | 2.5-4 | 10-16 | 1.0-1.5:1 |
| Entryway / foyer | 2.0x2.0 to 3.0x3.5 | 4-8 | 16-32 | 1.0-1.5:1 |
| Hallway / corridor | 1.0-1.2m wide | N/A (width) | 2-3 cells wide | Long/narrow |
| Dining room | 3.0x3.5 to 4.5x5.5 | 10-22 | 40-88 | 1.0-1.4:1 |
| Garage (single) | 3.5x6.0 | 20-24 | 80-96 | 1.6-1.8:1 |
| Garage (double) | 6.0x6.0 to 7.0x7.0 | 36-50 | 144-200 | 1.0-1.2:1 |
| Laundry room | 2.0x2.0 to 2.5x3.0 | 4-8 | 16-32 | 1.0-1.3:1 |
| Walk-in closet | 1.5x2.0 to 2.5x3.0 | 3-6 | 12-24 | 1.0-1.5:1 |
| Pantry | 1.0x1.5 to 2.0x2.5 | 1.5-4 | 6-16 | 1.0-1.5:1 |

### Floor-by-Floor Rules (1-2 story house)

**Ground Floor (mandatory):**
- Living room, kitchen, entryway (ALWAYS)
- At least one bathroom or half bath
- Dining room (if present)
- Garage (if present, ONLY ground floor)
- Laundry room (ground or basement)

**Upper Floor (if 2-story):**
- All or most bedrooms
- At least one full bathroom
- Master bedroom (can be ground or upper)
- Walk-in closets near bedrooms

**Rooms that NEVER go on upper floor:**
- Garage
- Kitchen (unless separate apartment)
- Main entryway

### Adjacency Rules
- Entryway MUST connect to living room or corridor
- Kitchen SHOULD be adjacent to dining room (strong)
- Kitchen SHOULD NOT be adjacent to bedrooms (noise)
- Bathrooms SHOULD be near bedrooms (preferred)
- Master bathroom SHOULD be attached to master bedroom (strong)
- Garage SHOULD connect to kitchen or entryway area (preferred)
- Closets SHOULD be attached to their bedroom (strong)
- Living room SHOULD be near entry (required)

### Recommended Archetype Values

```json
{
  "name": "residential_house",
  "rooms": [
    {"type": "living_room", "min_area": 72, "max_area": 120, "required": true, "priority": 1, "exterior_wall": true},
    {"type": "kitchen", "min_area": 48, "max_area": 80, "required": true, "priority": 2, "exterior_wall": true},
    {"type": "master_bedroom", "min_area": 72, "max_area": 112, "required": true, "priority": 3, "exterior_wall": true},
    {"type": "bedroom", "min_area": 40, "max_area": 72, "count": [1, 2], "required": true, "priority": 4, "exterior_wall": true},
    {"type": "bathroom", "min_area": 20, "max_area": 32, "count": [1, 2], "required": true, "priority": 5},
    {"type": "half_bath", "min_area": 10, "max_area": 16, "count": [0, 1], "required": false, "priority": 8},
    {"type": "entryway", "min_area": 16, "max_area": 32, "required": true, "priority": 6, "exterior_wall": true},
    {"type": "dining_room", "min_area": 40, "max_area": 72, "count": [0, 1], "required": false, "priority": 7},
    {"type": "garage", "min_area": 80, "max_area": 160, "count": [0, 1], "required": false, "priority": 9, "exterior_wall": true},
    {"type": "laundry", "min_area": 16, "max_area": 28, "count": [0, 1], "required": false, "priority": 10},
    {"type": "closet", "min_area": 8, "max_area": 20, "count": [0, 3], "required": false, "priority": 11},
    {"type": "corridor", "auto_generate": true}
  ]
}
```

---

## 2. Small Clinic / Medical Office

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio |
|------|-------------------|-----------------|---------------------|--------------|
| Waiting room | 4.0x5.0 to 6.0x8.0 | 20-45 | 80-180 | 1.0-1.4:1 |
| Reception desk area | 3.0x3.0 to 4.0x5.0 | 9-18 | 36-72 | 1.0-1.5:1 |
| Exam room | 3.0x3.0 to 3.5x4.0 | 9-12 | 36-48 | 1.0-1.2:1 |
| Doctor's office | 3.0x3.5 to 4.0x5.0 | 10-18 | 40-72 | 1.0-1.3:1 |
| Nurse station (1-person) | 2.5x3.0 to 3.0x3.5 | 7-10 | 28-40 | 1.0-1.2:1 |
| Supply / storage room | 2.5x3.0 to 3.0x4.0 | 7-12 | 28-48 | 1.0-1.4:1 |
| ADA restroom (single) | 2.0x2.5 to 2.5x3.0 | 5-7 | 20-28 | 1.0-1.2:1 |
| Staff break room | 3.0x3.5 to 4.0x4.5 | 10-16 | 40-64 | 1.0-1.3:1 |
| Corridor (ADA) | 1.5m wide minimum | N/A | 3 cells wide min | Long/narrow |

**Note:** Medical corridors MUST be minimum 1.83m (6 ft / ~4 cells) wide if gurney traffic is expected. Standard ADA clinic corridor = 1.52m (5 ft / 3 cells).

### Floor-by-Floor Rules (typically 1 floor)

**Ground Floor (all rooms):**
- Waiting room at entrance (ALWAYS)
- Reception between waiting room and clinical areas
- Exam rooms off corridor (clinical wing)
- At least 1 ADA restroom accessible from waiting area
- Staff areas (break room, offices) separated from patient areas

### Adjacency Rules
- Waiting room MUST be at building entrance (required)
- Reception MUST be adjacent to waiting room (required)
- Reception MUST control access to clinical corridor (required)
- Exam rooms MUST be off main corridor (required)
- Supply room SHOULD be near exam rooms (strong)
- Restroom SHOULD be accessible from waiting area (strong)
- Break room SHOULD be in staff-only area (preferred)
- Doctor's office SHOULD be near exam rooms (preferred)

### Recommended Archetype Values

```json
{
  "name": "clinic",
  "rooms": [
    {"type": "waiting_room", "min_area": 80, "max_area": 160, "required": true, "priority": 1, "exterior_wall": true},
    {"type": "reception", "min_area": 36, "max_area": 64, "required": true, "priority": 2},
    {"type": "exam_room", "min_area": 36, "max_area": 48, "count": [2, 4], "required": true, "priority": 3},
    {"type": "doctor_office", "min_area": 40, "max_area": 72, "count": [1, 2], "required": true, "priority": 4},
    {"type": "nurse_station", "min_area": 28, "max_area": 40, "count": [0, 1], "required": false, "priority": 7},
    {"type": "bathroom_ada", "min_area": 20, "max_area": 28, "count": [1, 2], "required": true, "priority": 5},
    {"type": "supply_room", "min_area": 28, "max_area": 48, "required": true, "priority": 6},
    {"type": "break_room", "min_area": 40, "max_area": 56, "required": false, "priority": 8},
    {"type": "corridor", "auto_generate": true}
  ]
}
```

---

## 3. Office Building (Multi-Floor)

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio |
|------|-------------------|-----------------|---------------------|--------------|
| Ground floor lobby | 5.0x6.0 to 8.0x12.0 | 30-80 | 120-320 | 1.0-1.5:1 |
| Reception desk area | 3.0x4.0 to 5.0x5.0 | 12-25 | 48-100 | 1.0-1.5:1 |
| Mail room | 3.0x4.0 to 4.0x5.0 | 12-20 | 48-80 | 1.0-1.3:1 |
| Security desk | 2.5x3.0 to 3.0x4.0 | 7-12 | 28-48 | 1.0-1.3:1 |
| Open office area | 8.0x10.0 to 12.0x15.0 | 50-150 | 200-600 | 1.0-1.5:1 |
| Private office | 3.0x3.5 to 4.0x5.0 | 10-18 | 40-72 | 1.0-1.3:1 |
| Executive office | 4.0x5.0 to 6.0x7.0 | 20-35 | 80-140 | 1.0-1.3:1 |
| Small conference (4-6) | 3.0x3.5 to 4.0x4.5 | 10-16 | 40-64 | 1.0-1.3:1 |
| Large conference (10-16) | 4.5x6.0 to 6.0x8.0 | 25-45 | 100-180 | 1.0-1.5:1 |
| Boardroom | 6.0x8.0 to 8.0x12.0 | 45-80 | 180-320 | 1.2-1.5:1 |
| Break room / kitchenette | 4.0x5.0 to 5.0x6.0 | 18-30 | 72-120 | 1.0-1.3:1 |
| Male restroom | 3.0x4.0 to 4.0x5.0 | 12-20 | 48-80 | 1.0-1.3:1 |
| Female restroom | 3.0x4.0 to 4.0x5.0 | 12-20 | 48-80 | 1.0-1.3:1 |
| Server / IT room | 3.0x4.0 to 4.0x5.0 | 12-20 | 48-80 | 1.0-1.3:1 |
| Utility closet | 1.5x2.0 to 2.0x3.0 | 3-6 | 12-24 | 1.0-1.5:1 |
| Elevator shaft | 2.0x2.0 to 2.5x2.5 | 4-6 | 16-24 | ~1.0:1 |
| Stairwell | 2.5x5.0 to 3.0x6.0 | 12-18 | 48-72 | 1.8-2.2:1 |
| Copy / print room | 2.5x3.0 to 3.0x4.0 | 7-12 | 28-48 | 1.0-1.3:1 |
| Corridor (commercial) | 1.5m wide minimum | N/A | 3 cells wide min | Long/narrow |

### Floor-by-Floor Rules (3-10 floors)

**Ground Floor (mandatory):**
- Lobby with reception (ALWAYS)
- Security desk (optional, larger buildings)
- Mail room (optional)
- Elevator + stairwell access
- At least one restroom pair (M/F)
- May include ground-floor retail/commercial space

**Typical Upper Floors (mandatory per-floor):**
- Elevator shaft (EVERY floor, same position)
- Stairwell (EVERY floor, same position, min 2 for fire code above 930 m²)
- Male restroom (EVERY floor)
- Female restroom (EVERY floor)
- Utility closet (EVERY floor)
- Open office OR private offices (primary use)
- At least one conference room per floor
- Break room (every 1-2 floors)

**Top Floor / Executive (optional):**
- Executive offices (larger than standard)
- Boardroom
- Executive restroom
- Executive break room / kitchen

**Rooms that MUST appear on EVERY floor:**
- Elevator, stairwell, restrooms (M+F), utility closet

**Rooms that should ONLY appear on ground floor:**
- Lobby, reception, mail room, security desk

**Rooms that should NOT appear on ground floor:**
- Open office (typically), server room (typically mid-building)

### Adjacency Rules
- Lobby MUST be at entrance (required)
- Reception MUST be in/adjacent to lobby (required)
- Elevator MUST be near lobby on ground floor (required)
- Stairwell MUST be near elevator (strong)
- Restrooms SHOULD be near stairwell/elevator core (strong)
- Conference rooms SHOULD be near reception/lobby on ground floor (preferred)
- Server room SHOULD NOT be on ground floor (flood risk) (preferred)
- Break room SHOULD NOT be adjacent to conference rooms (noise) (preferred)
- Utility closet SHOULD be near stairwell core (preferred)
- Copy room SHOULD be central to office areas (preferred)

### Recommended Archetype Values

```json
{
  "name": "office_building",
  "rooms_ground_floor": [
    {"type": "lobby", "min_area": 120, "max_area": 280, "required": true, "priority": 1, "exterior_wall": true},
    {"type": "reception", "min_area": 48, "max_area": 80, "required": true, "priority": 2},
    {"type": "mail_room", "min_area": 40, "max_area": 64, "required": false, "priority": 8},
    {"type": "security_desk", "min_area": 28, "max_area": 40, "required": false, "priority": 9},
    {"type": "restroom_male", "min_area": 48, "max_area": 72, "required": true, "priority": 5},
    {"type": "restroom_female", "min_area": 48, "max_area": 72, "required": true, "priority": 5},
    {"type": "elevator", "min_area": 16, "max_area": 24, "required": true, "priority": 3, "fixed_position": true},
    {"type": "stairwell", "min_area": 48, "max_area": 64, "required": true, "priority": 3, "fixed_position": true},
    {"type": "utility_closet", "min_area": 12, "max_area": 20, "required": true, "priority": 10},
    {"type": "corridor", "auto_generate": true}
  ],
  "rooms_upper_floor": [
    {"type": "open_office", "min_area": 200, "max_area": 480, "required": true, "priority": 1},
    {"type": "private_office", "min_area": 40, "max_area": 64, "count": [2, 6], "required": true, "priority": 2},
    {"type": "conference_room", "min_area": 48, "max_area": 120, "count": [1, 2], "required": true, "priority": 3},
    {"type": "break_room", "min_area": 60, "max_area": 100, "required": true, "priority": 6},
    {"type": "restroom_male", "min_area": 48, "max_area": 72, "required": true, "priority": 4},
    {"type": "restroom_female", "min_area": 48, "max_area": 72, "required": true, "priority": 4},
    {"type": "copy_room", "min_area": 24, "max_area": 40, "required": false, "priority": 8},
    {"type": "server_room", "min_area": 40, "max_area": 64, "count": [0, 1], "required": false, "priority": 9},
    {"type": "elevator", "min_area": 16, "max_area": 24, "required": true, "priority": 3, "fixed_position": true},
    {"type": "stairwell", "min_area": 48, "max_area": 64, "required": true, "priority": 3, "fixed_position": true},
    {"type": "utility_closet", "min_area": 12, "max_area": 20, "required": true, "priority": 10},
    {"type": "corridor", "auto_generate": true}
  ],
  "rooms_top_floor": [
    {"type": "executive_office", "min_area": 80, "max_area": 140, "count": [1, 3], "required": true, "priority": 1},
    {"type": "boardroom", "min_area": 140, "max_area": 280, "required": true, "priority": 2},
    {"type": "private_office", "min_area": 40, "max_area": 64, "count": [2, 4], "required": true, "priority": 3},
    {"type": "conference_room", "min_area": 48, "max_area": 100, "count": [0, 1], "required": false, "priority": 5},
    {"type": "break_room", "min_area": 60, "max_area": 100, "required": true, "priority": 6},
    {"type": "restroom_male", "min_area": 48, "max_area": 72, "required": true, "priority": 4},
    {"type": "restroom_female", "min_area": 48, "max_area": 72, "required": true, "priority": 4},
    {"type": "elevator", "min_area": 16, "max_area": 24, "required": true, "priority": 3, "fixed_position": true},
    {"type": "stairwell", "min_area": 48, "max_area": 64, "required": true, "priority": 3, "fixed_position": true},
    {"type": "utility_closet", "min_area": 12, "max_area": 20, "required": true, "priority": 10},
    {"type": "corridor", "auto_generate": true}
  ]
}
```

---

## 4. Police Station

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio | Notes |
|------|-------------------|-----------------|---------------------|--------------|-------|
| Public lobby | 5.0x6.0 to 7.0x8.0 | 30-50 | 120-200 | 1.0-1.3:1 | Bulletproof reception window |
| Front desk / reception | 3.0x4.0 to 4.0x5.0 | 12-18 | 48-72 | 1.0-1.3:1 | Behind partition |
| Waiting area | 3.0x4.0 to 5.0x5.0 | 12-25 | 48-100 | 1.0-1.3:1 | Public side |
| Bullpen / detective area | 8.0x10.0 to 12.0x15.0 | 60-120 | 240-480 | 1.0-1.5:1 | Open desk area |
| Interrogation room | 3.0x3.5 to 3.5x4.5 | 10-15 | 40-60 | 1.0-1.3:1 | Typically ~14 m² (150 sq ft) |
| Observation room | 2.0x3.0 to 2.5x3.5 | 6-8 | 24-32 | 1.2-1.4:1 | Adjacent to interrogation |
| Holding cell (individual) | 2.0x2.5 to 2.5x3.0 | 5-7 | 20-28 | 1.0-1.2:1 | Minimum 4.6 m² (IBC) |
| Holding cell (group) | 3.0x4.0 to 4.0x6.0 | 12-20 | 48-80 | 1.0-1.5:1 | |
| Booking area | 4.0x5.0 to 5.0x7.0 | 18-30 | 72-120 | 1.0-1.4:1 | Counter, fingerprint station |
| Evidence room | 4.0x5.0 to 6.0x8.0 | 20-40 | 80-160 | 1.0-1.5:1 | Secure, shelved |
| Armory | 2.5x3.0 to 3.5x4.5 | 7-14 | 28-56 | 1.0-1.3:1 | Secure, reinforced |
| Locker room | 4.0x5.0 to 5.0x7.0 | 20-30 | 80-120 | 1.0-1.4:1 | With shower area |
| Chief's office | 4.0x4.5 to 5.0x6.0 | 18-28 | 72-112 | 1.0-1.3:1 | |
| Detective office | 3.0x3.5 to 4.0x5.0 | 10-18 | 40-72 | 1.0-1.3:1 | |
| Dispatch / comms | 4.0x5.0 to 5.0x6.0 | 18-28 | 72-112 | 1.0-1.2:1 | Multiple stations |
| Break room | 4.0x5.0 to 5.0x6.0 | 18-28 | 72-112 | 1.0-1.3:1 | |
| Restroom | 2.5x3.0 to 3.5x4.0 | 7-12 | 28-48 | 1.0-1.3:1 | |
| Corridor | 1.5m wide minimum | N/A | 3 cells wide min | Long/narrow | Secure/public separation |

### Floor-by-Floor Rules (1-2 floors)

**Ground Floor (mandatory):**
- Public lobby + front desk (ALWAYS at entrance)
- Waiting area (public side)
- Booking area (behind secure partition)
- Holding cells (behind secure area, near booking)
- Dispatch / communications
- At least 2 restrooms (1 public, 1 staff)
- Armory (ground floor, secure, no windows)

**Upper Floor (if 2-story):**
- Bullpen / detective area
- Chief's office
- Detective offices
- Interrogation rooms + observation rooms
- Evidence room (secure, often upper floor or basement)
- Locker room
- Break room
- Restrooms

**Critical security zones:**
- Clear PUBLIC / SECURE boundary between lobby and operational areas
- Holding cells MUST be in secure zone with controlled access
- Evidence room MUST be separate secure zone
- Armory MUST be separate secure zone
- Interrogation adjacent to observation (one-way glass wall)

### Adjacency Rules
- Lobby MUST be at exterior entrance (required)
- Front desk MUST be between lobby and secure area (required)
- Holding cells MUST be adjacent to booking (required)
- Interrogation MUST be adjacent to observation room (required)
- Bullpen SHOULD be adjacent to chief's office (strong)
- Dispatch SHOULD be near front desk (strong)
- Evidence room SHOULD NOT be adjacent to public areas (required)
- Armory SHOULD NOT be adjacent to public areas (required)
- Locker room SHOULD be near break room (preferred)
- Restrooms SHOULD be accessible from both public and secure zones (strong)

### Recommended Archetype Values

```json
{
  "name": "police_station",
  "rooms": [
    {"type": "lobby", "min_area": 120, "max_area": 200, "required": true, "priority": 1, "exterior_wall": true},
    {"type": "front_desk", "min_area": 48, "max_area": 72, "required": true, "priority": 2},
    {"type": "waiting_area", "min_area": 48, "max_area": 80, "required": true, "priority": 3, "exterior_wall": true},
    {"type": "bullpen", "min_area": 200, "max_area": 400, "required": true, "priority": 4},
    {"type": "chief_office", "min_area": 72, "max_area": 112, "required": true, "priority": 5},
    {"type": "detective_office", "min_area": 40, "max_area": 64, "count": [1, 3], "required": false, "priority": 8},
    {"type": "interrogation", "min_area": 40, "max_area": 56, "count": [1, 3], "required": true, "priority": 6},
    {"type": "observation", "min_area": 24, "max_area": 32, "count": [1, 2], "required": false, "priority": 9},
    {"type": "holding_cell", "min_area": 20, "max_area": 28, "count": [2, 4], "required": true, "priority": 7},
    {"type": "booking", "min_area": 72, "max_area": 120, "required": true, "priority": 5},
    {"type": "evidence_room", "min_area": 80, "max_area": 160, "required": true, "priority": 6},
    {"type": "armory", "min_area": 28, "max_area": 48, "required": true, "priority": 8},
    {"type": "locker_room", "min_area": 80, "max_area": 120, "required": true, "priority": 9},
    {"type": "dispatch", "min_area": 72, "max_area": 112, "required": true, "priority": 4},
    {"type": "break_room", "min_area": 72, "max_area": 100, "required": true, "priority": 10},
    {"type": "bathroom", "min_area": 28, "max_area": 48, "count": [2, 3], "required": true, "priority": 11},
    {"type": "corridor", "auto_generate": true}
  ]
}
```

---

## 5. Apartment Building (Multi-Unit)

### Individual Unit Sizes

| Unit Type | Total Area (m²) | Grid Cells | Typical Layout |
|-----------|-----------------|-----------|----------------|
| Studio | 25-40 | 100-160 | Combined living/sleeping + kitchen + bath |
| 1-Bedroom | 40-60 | 160-240 | Living + kitchen + 1BR + bath |
| 2-Bedroom | 60-90 | 240-360 | Living + kitchen + 2BR + bath |

### Room Dimensions Within Units

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio |
|------|-------------------|-----------------|---------------------|--------------|
| Living room (1BR) | 3.5x4.0 to 4.5x5.5 | 14-22 | 56-88 | 1.0-1.4:1 |
| Living room (2BR) | 4.0x5.0 to 5.0x6.0 | 18-28 | 72-112 | 1.0-1.3:1 |
| Kitchen (apartment) | 2.5x3.0 to 3.5x4.0 | 7-12 | 28-48 | 1.0-1.4:1 |
| Kitchenette (studio) | 2.0x2.5 to 2.5x3.0 | 5-7 | 20-28 | 1.0-1.2:1 |
| Bedroom (apartment) | 3.0x3.5 to 4.0x4.5 | 10-16 | 40-64 | 1.0-1.3:1 |
| Bathroom (apartment) | 1.8x2.5 to 2.5x3.0 | 4.5-7 | 18-28 | 1.0-1.3:1 |
| Studio main room | 4.5x5.5 to 5.5x7.0 | 25-35 | 100-140 | 1.0-1.3:1 |
| Unit entryway | 1.0x1.5 to 1.5x2.5 | 1.5-3 | 6-12 | 1.0-1.5:1 |

### Common Areas (Building-Level)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio |
|------|-------------------|-----------------|---------------------|--------------|
| Building lobby | 4.0x5.0 to 6.0x8.0 | 20-40 | 80-160 | 1.0-1.4:1 |
| Mailbox area | 2.0x3.0 to 3.0x4.0 | 6-10 | 24-40 | 1.0-1.5:1 |
| Laundry room | 3.0x4.0 to 4.0x5.0 | 12-20 | 48-80 | 1.0-1.3:1 |
| Building corridor | 1.5m wide (ADA) | N/A | 3 cells wide | Long/narrow |
| Stairwell | 2.5x5.0 to 3.0x6.0 | 12-18 | 48-72 | ~2.0:1 |
| Elevator shaft | 2.0x2.0 to 2.5x2.5 | 4-6 | 16-24 | ~1.0:1 |
| Storage cages | 2.0x2.0 to 2.5x3.0 | 4-6 | 16-24 | 1.0-1.3:1 |

### Floor-by-Floor Rules

**Ground Floor (mandatory):**
- Building lobby with mailboxes (ALWAYS at entrance)
- Stairwell + elevator access
- Laundry room (ground floor or basement)
- May have 1-2 apartment units
- Optional: super/manager office

**Upper Floors (typical):**
- Central corridor with units on each side
- Stairwell + elevator (EVERY floor, same position)
- 2-6 units per floor depending on building size
- Each unit is self-contained (living + kitchen + bedroom(s) + bathroom)

**Layout pattern:** Double-loaded corridor (units on both sides of a central hallway) is the most space-efficient and common pattern.

### Recommended Archetype Values (Single Unit)

```json
{
  "name": "apartment_1br",
  "rooms": [
    {"type": "living_room", "min_area": 56, "max_area": 88, "required": true, "priority": 1, "exterior_wall": true},
    {"type": "kitchen", "min_area": 28, "max_area": 48, "required": true, "priority": 2, "exterior_wall": true},
    {"type": "bedroom", "min_area": 40, "max_area": 64, "required": true, "priority": 3, "exterior_wall": true},
    {"type": "bathroom", "min_area": 18, "max_area": 28, "required": true, "priority": 4},
    {"type": "entryway", "min_area": 8, "max_area": 16, "required": true, "priority": 5},
    {"type": "closet", "min_area": 8, "max_area": 16, "count": [0, 2], "required": false, "priority": 6},
    {"type": "corridor", "auto_generate": true}
  ]
}
```

---

## 6. Retail / Commercial

### 6a. Retail Shop

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio | Notes |
|------|-------------------|-----------------|---------------------|--------------|-------|
| Sales floor / storefront | 6.0x8.0 to 10.0x15.0 | 50-120 | 200-480 | 1.0-2.0:1 | Open, with display fixtures |
| Back room / storage | 3.0x4.0 to 5.0x6.0 | 12-30 | 48-120 | 1.0-1.5:1 | ~15-30% of total area |
| Office | 2.5x3.0 to 3.5x4.0 | 7-12 | 28-48 | 1.0-1.3:1 | Manager's office |
| Restroom (staff) | 1.5x2.0 to 2.0x2.5 | 3-5 | 12-20 | 1.0-1.3:1 | |
| Restroom (ADA, public) | 2.0x2.5 to 2.5x3.0 | 5-7 | 20-28 | 1.0-1.2:1 | If public restroom required |

**Floor rules:** Almost always single floor. Sales floor at front (storefront windows), back room behind, office and restrooms in back.

### 6b. Restaurant

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio | Notes |
|------|-------------------|-----------------|---------------------|--------------|-------|
| Dining area | 6.0x8.0 to 10.0x14.0 | 50-120 | 200-480 | 1.0-1.5:1 | ~60% of total |
| Kitchen | 4.0x6.0 to 6.0x8.0 | 25-45 | 100-180 | 1.0-1.5:1 | ~30% of total |
| Bar area | 3.0x6.0 to 4.0x8.0 | 18-30 | 72-120 | 1.5-2.5:1 | Long and narrow |
| Restroom (male) | 2.0x2.5 to 3.0x3.5 | 5-10 | 20-40 | 1.0-1.3:1 | |
| Restroom (female) | 2.0x2.5 to 3.0x3.5 | 5-10 | 20-40 | 1.0-1.3:1 | |
| Storage / walk-in | 2.5x3.0 to 3.5x4.0 | 7-14 | 28-56 | 1.0-1.3:1 | Cold + dry storage |
| Office | 2.5x3.0 to 3.0x3.5 | 7-10 | 28-40 | 1.0-1.2:1 | |
| Vestibule / entry | 2.0x2.0 to 2.5x3.0 | 4-6 | 16-24 | 1.0-1.3:1 | |

**Floor rules:** Single floor. Dining at front with windows. Kitchen at rear. Bar can be front or middle. Restrooms accessible from dining area but out of sight. Kitchen MUST have back door for deliveries.

### 6c. Bar

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio |
|------|-------------------|-----------------|---------------------|--------------|
| Bar / main area | 6.0x8.0 to 10.0x12.0 | 50-100 | 200-400 | 1.0-1.5:1 |
| Seating / lounge | 4.0x5.0 to 6.0x8.0 | 20-45 | 80-180 | 1.0-1.4:1 |
| Kitchen / prep | 3.0x4.0 to 4.0x5.0 | 12-20 | 48-80 | 1.0-1.3:1 |
| Restroom (male) | 2.0x2.5 to 3.0x3.5 | 5-10 | 20-40 | 1.0-1.3:1 |
| Restroom (female) | 2.0x2.5 to 3.0x3.5 | 5-10 | 20-40 | 1.0-1.3:1 |
| Storage | 2.5x3.0 to 3.0x4.0 | 7-12 | 28-48 | 1.0-1.3:1 |
| Office | 2.0x2.5 to 3.0x3.0 | 5-9 | 20-36 | 1.0-1.2:1 |

---

## 7. Warehouse / Industrial

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio | Notes |
|------|-------------------|-----------------|---------------------|--------------|-------|
| Main warehouse floor | 15.0x20.0 to 30.0x40.0 | 300-800+ | 1200-3200+ | 1.0-2.0:1 | Open, high ceiling |
| Loading dock area | 4.0x10.0 to 6.0x12.0 | 40-60 | 160-240 | 2.0-3.0:1 | Along exterior wall |
| Office | 3.0x3.5 to 4.0x5.0 | 10-18 | 40-72 | 1.0-1.3:1 | Small, near entrance |
| Restroom | 2.0x2.5 to 3.0x3.5 | 5-10 | 20-40 | 1.0-1.3:1 | |
| Break room | 3.5x4.0 to 5.0x6.0 | 14-25 | 56-100 | 1.0-1.3:1 | |
| Storage cage / secure | 3.0x3.0 to 4.0x5.0 | 9-20 | 36-80 | 1.0-1.3:1 | Caged area within warehouse |

### Floor-by-Floor Rules (1 floor, high ceiling)

- Almost always single-story
- Warehouse floor is 80-90% of building area
- Office, break room, restrooms in a small cluster near main entrance
- Loading docks along one exterior wall (usually rear)
- Office area may have mezzanine (second-floor balcony)

### Adjacency Rules
- Office SHOULD be near main entrance (required)
- Break room SHOULD be near restrooms (strong)
- Loading dock MUST be on exterior wall (required)
- Restrooms SHOULD be accessible from warehouse floor (strong)

---

## 8. Church / Chapel

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio | Notes |
|------|-------------------|-----------------|---------------------|--------------|-------|
| Nave (main hall) | 8.0x15.0 to 12.0x25.0 | 100-250 | 400-1000 | 1.5-2.5:1 | Long, narrow hall |
| Altar / chancel area | 4.0x5.0 to 6.0x8.0 | 20-40 | 80-160 | 1.0-1.3:1 | At end of nave |
| Vestibule / narthex | 3.0x8.0 to 5.0x12.0 | 24-50 | 96-200 | 2.0-3.0:1 | Wide, shallow at entrance |
| Office / sacristy | 3.0x4.0 to 4.0x5.0 | 12-18 | 48-72 | 1.0-1.3:1 | Off altar area |
| Restrooms | 2.0x2.5 to 3.0x3.5 | 5-10 | 20-40 | 1.0-1.3:1 | |
| Fellowship hall | 6.0x8.0 to 10.0x14.0 | 50-120 | 200-480 | 1.0-1.5:1 | May be basement |
| Small chapel | 4.0x6.0 to 5.0x8.0 | 24-36 | 96-144 | 1.3-1.6:1 | Side chapel |
| Choir loft / balcony | 3.0x8.0 to 4.0x10.0 | 24-36 | 96-144 | 2.5-3.0:1 | Above vestibule |

### Floor-by-Floor Rules (1-2 floors)

**Ground Floor (mandatory):**
- Vestibule / narthex at entrance (ALWAYS)
- Nave (main worship space)
- Altar / chancel at far end from entrance
- Restrooms accessible from vestibule
- Office / sacristy near altar

**Basement (optional):**
- Fellowship hall
- Kitchen (for events)
- Classrooms / Sunday school rooms
- Storage

### Adjacency Rules
- Vestibule MUST be at entrance (required)
- Nave MUST be directly beyond vestibule (required)
- Altar MUST be at far end of nave from entrance (required)
- Sacristy MUST be adjacent to altar area (required)
- Restrooms SHOULD be near vestibule (strong)
- Fellowship hall SHOULD be accessible without going through nave (preferred)

---

## 9. School

### Room Dimensions (Real-World)

| Room | Width x Depth (m) | Area Range (m²) | Grid Cells (min-max) | Aspect Ratio | Notes |
|------|-------------------|-----------------|---------------------|--------------|-------|
| Classroom | 8.0x9.0 to 9.0x10.0 | 65-85 | 260-340 | 1.0-1.2:1 | ~2.3 m² per student |
| Hallway / corridor | 2.5-4.5m wide | N/A | 5-9 cells wide | Long/narrow | Lockers on walls |
| Admin office suite | 4.0x5.0 to 6.0x8.0 | 20-40 | 80-160 | 1.0-1.4:1 | Principal + secretary |
| Gymnasium | 15.0x25.0 to 20.0x30.0 | 350-550 | 1400-2200 | 1.5-1.8:1 | High ceiling |
| Cafeteria | 10.0x15.0 to 15.0x20.0 | 150-250 | 600-1000 | 1.3-1.5:1 | With serving line |
| Kitchen (cafeteria) | 5.0x8.0 to 7.0x10.0 | 40-60 | 160-240 | 1.3-1.5:1 | Behind cafeteria |
| Library / media center | 8.0x10.0 to 12.0x15.0 | 80-150 | 320-600 | 1.0-1.4:1 | |
| Restroom block | 3.0x5.0 to 4.0x6.0 | 15-24 | 60-96 | 1.3-1.5:1 | Per floor, M+F |
| Teacher's lounge | 4.0x5.0 to 5.0x6.0 | 20-28 | 80-112 | 1.0-1.3:1 | |
| Nurse's office | 3.0x4.0 to 4.0x5.0 | 12-18 | 48-72 | 1.0-1.3:1 | |
| Janitor closet | 1.5x2.0 to 2.0x2.5 | 3-5 | 12-20 | 1.0-1.3:1 | Every floor |
| Stairwell | 2.5x5.0 to 3.0x6.0 | 12-18 | 48-72 | ~2.0:1 | |

### Floor-by-Floor Rules (1-3 floors)

**Ground Floor (mandatory):**
- Main entrance / lobby
- Admin office near entrance
- Nurse's office near entrance
- Gymnasium (ground floor, exterior access)
- Cafeteria + kitchen (ground floor, delivery access)
- Classrooms
- Restrooms (M+F)
- Library (often ground floor for accessibility)

**Upper Floors:**
- Classrooms (primary use)
- Restrooms (EVERY floor, M+F)
- Stairwell (EVERY floor, min 2)
- Teacher's lounge (one floor)
- Janitor closet (EVERY floor)

**Rooms that should be GROUND FLOOR ONLY:**
- Gymnasium, cafeteria/kitchen, main office, nurse's office

### Adjacency Rules
- Admin office MUST be near main entrance (required)
- Nurse's office SHOULD be near admin (strong)
- Cafeteria MUST be adjacent to kitchen (required)
- Gymnasium SHOULD be near cafeteria (they share events) (preferred)
- Library SHOULD be accessible from main corridor (strong)
- Restrooms SHOULD be distributed, not clustered (strong)
- Classrooms SHOULD NOT be adjacent to gymnasium (noise) (preferred)

---

## Universal Building Code Rules

These rules apply to ALL building types and should be enforced by the floor plan generator.

### Minimum Room Sizes (IBC)

| Rule | Value | Notes |
|------|-------|-------|
| Minimum habitable room (primary) | 11.1 m² (120 sq ft) = 44 cells | At least one room per dwelling must meet this |
| Minimum habitable room (other) | 6.5 m² (70 sq ft) = 26 cells | Other habitable rooms |
| Minimum ceiling height (habitable) | 2.3 m (7'6") | Standard |
| Minimum ceiling height (bath/kitchen) | 2.1 m (7'0") | Reduced allowance |

### Corridor Widths

| Context | Minimum Width | In Cells (50cm) |
|---------|--------------|-----------------|
| Residential interior | 0.91 m (36") | 2 cells (tight) |
| Commercial (occupancy < 50) | 0.91 m (36") | 2 cells |
| Commercial (standard) | 1.12 m (44") | 3 cells (round up) |
| ADA accessible route | 0.91 m (36") | 2 cells |
| ADA with passing space | 1.52 m (60") | 3 cells |
| Medical / gurney traffic | 1.83 m (72") | 4 cells |
| School corridor (high capacity) | 1.83 m (72") | 4 cells |
| School corridor (100+ capacity) | 1.83-2.44 m (72-96") | 4-5 cells |

**Recommendation for procgen:** Default corridor width of 3 cells (1.5m) for most buildings. 4 cells for schools and medical. 2 cells only for residential interiors.

### Door Widths

| Context | Minimum Width | Notes |
|---------|--------------|-------|
| Residential interior | 0.76 m (30") | Bedroom, bathroom doors |
| Residential entry | 0.91 m (36") | Front door |
| Commercial standard | 0.91 m (36") | Most commercial doors |
| ADA minimum clear | 0.81 m (32") | Measured with door open 90 deg |
| ADA (deep opening) | 0.91 m (36") | Openings deeper than 61cm |
| Double door (commercial) | 1.52 m (60") | Main entrance |

**Current Leviathan capsule:** 42cm radius = 84cm diameter. The existing 90cm door width is passable but tight. **Recommend 100cm minimum** for comfortable gameplay (see door-clearance-research.md).

### Fire Egress

| Rule | Value | Notes |
|------|-------|-------|
| Two means of egress required | Occupancy > 49 OR floor area > 93 m² (1000 sq ft) | Most commercial buildings |
| Maximum dead-end corridor | 6.1 m (20 ft) | Without sprinklers |
| Maximum dead-end corridor (sprinklered) | 15.2 m (50 ft) | With sprinklers |
| Stairwell: min width | 1.12 m (44") = 3 cells | Standard |
| Stairwell: min headroom | 2.03 m (80") | Measured vertically |

**Recommendation for procgen:** Buildings with floor area > 200 cells (50 m²) per floor should have 2 stairwells. For game purposes, most multi-story buildings should have 2 stairwells for interesting navigation.

### ADA Requirements

| Rule | Value |
|------|-------|
| Accessible route width | 0.91 m (36") minimum |
| Passing space (every 60m) | 1.52 m x 1.52 m (5' x 5') |
| ADA restroom turning radius | 1.52 m (60") |
| ADA restroom minimum | 5.2 m² (~21 cells) |
| Ramp maximum slope | 1:12 |
| Ramp maximum rise per run | 0.76 m (30") |

---

## Universal Adjacency Rules

These common-sense architectural rules should be enforced across all building types.

### Mandatory Rules (enforce these)

1. **Every building needs at least one exterior entrance** connecting to the entryway/lobby/vestibule
2. **Every floor above ground needs stairs** (minimum), ideally elevator too
3. **Stairwells and elevators occupy the same grid position on every floor** (vertical alignment)
4. **Commercial buildings need restrooms on every floor** (M+F or unisex)
5. **Lobbies only on ground floor** (not duplicated on upper floors)
6. **Kitchen never adjacent to bedroom** (noise, residential)
7. **Restrooms never adjacent to kitchen** (hygiene, commercial)
8. **Bathrooms share plumbing walls when possible** (stack vertically, cluster horizontally)

### Strong Preferences (try hard to satisfy)

1. **Service rooms (restrooms, utility, janitor) cluster near building core** (stairwell/elevator)
2. **Public rooms near entrance** (lobby, waiting, reception)
3. **Private/secure rooms away from entrance** (evidence, armory, server room)
4. **Noisy rooms away from quiet rooms** (gym vs classroom, bar vs office)
5. **Rooms requiring daylight on exterior walls** (offices, bedrooms, living rooms)
6. **Support rooms interior** (storage, utility, restrooms don't need windows)
7. **Corridors as spines, not mazes** (single main corridor per floor, rooms branch off)

### Weak Preferences (nice to have)

1. **Similar rooms grouped** (all bedrooms in one wing, all offices together)
2. **Progression from public to private** (as you go deeper into building)
3. **Staff areas separated from public areas** (break room not near waiting room)

---

## Archetype Format Extensions Needed

The current archetype JSON format needs several extensions to support realistic floor-by-floor generation.

### 1. Per-Floor Room Definitions

**Current:** Single `rooms` array, all rooms go on all floors.
**Needed:** `rooms_ground_floor`, `rooms_upper_floor`, `rooms_top_floor` (or a `floor_assignment` field per room).

Proposed approach (minimal change):
```json
{
  "rooms": [
    {"type": "lobby", "min_area": 120, "max_area": 280, "floor": "ground", ...},
    {"type": "open_office", "min_area": 200, "max_area": 480, "floor": "upper", ...},
    {"type": "stairwell", "min_area": 48, "max_area": 64, "floor": "every", "fixed_position": true, ...},
    {"type": "restroom", "min_area": 48, "max_area": 72, "floor": "every", ...}
  ]
}
```

**`floor` values:** `"ground"`, `"upper"`, `"top"`, `"every"`, `"any"` (default, current behavior).

### 2. Aspect Ratio Constraints

**Current:** No aspect ratio control. Treemap can produce very elongated rooms.
**Needed:** `min_aspect` and `max_aspect` per room type.

```json
{"type": "corridor", "auto_generate": true, "min_aspect": 3.0, "max_aspect": 20.0},
{"type": "bathroom", "min_aspect": 1.0, "max_aspect": 1.5},
{"type": "nave", "min_aspect": 1.5, "max_aspect": 2.5}
```

### 3. Fixed-Position Rooms

**Current:** Room positions are all relative.
**Needed:** Stairwells and elevators must occupy the same grid cells on every floor.

```json
{"type": "stairwell", "fixed_position": true, "floor": "every"}
```

### 4. Corridor Width Override

**Current:** Single corridor width for entire building.
**Needed:** Per-building-type corridor width specification.

```json
{
  "corridor_width_cells": 3,
  "corridor_min_width_cells": 3
}
```

### 5. Security Zones

For police stations and similar buildings, rooms need zone assignments:
```json
{"type": "holding_cell", "zone": "secure", ...},
{"type": "lobby", "zone": "public", ...}
```

Zones prevent adjacency between incompatible areas (public rooms should not directly connect to secure rooms without an access-controlled corridor).

### 6. Vertical Plumbing Stacking

Hint to the generator that bathrooms/restrooms should align vertically across floors:
```json
{"type": "restroom", "stack_vertically": true}
```

---

## Building Footprint Recommendations

Given the realistic room sizes above, here are minimum building footprints per type:

| Building Type | Min Footprint (m) | Min Grid Cells | Total Floor Area (m²) |
|---------------|-------------------|----------------|----------------------|
| Residential house (3BR) | 10x12 = 120 m² | 20x24 = 480 | 120-200 (1-2 floors) |
| Small clinic | 12x15 = 180 m² | 24x30 = 720 | 180-250 |
| Office building (per floor) | 15x20 = 300 m² | 30x40 = 1200 | 300-500 per floor |
| Police station | 15x25 = 375 m² | 30x50 = 1500 | 375-600 (1-2 floors) |
| Apartment unit (1BR) | 6x8 = 48 m² | 12x16 = 192 | 48-65 per unit |
| Apartment building (per floor) | 12x30 = 360 m² | 24x60 = 1440 | 360 per floor |
| Retail shop | 8x15 = 120 m² | 16x30 = 480 | 120-200 |
| Restaurant | 10x18 = 180 m² | 20x36 = 720 | 180-300 |
| Bar | 8x12 = 96 m² | 16x24 = 384 | 96-180 |
| Warehouse | 20x30 = 600 m² | 40x60 = 2400 | 600-1200+ |
| Church / chapel | 12x25 = 300 m² | 24x50 = 1200 | 300-500 |
| School (per floor) | 20x40 = 800 m² | 40x80 = 3200 | 800-1500 per floor |

**Note:** The treemap generator needs footprint dimensions as input. If the caller provides a footprint that's too small for the archetype's rooms, the generator should either warn or scale rooms down proportionally. A validation step that compares total room area to footprint area would prevent impossible layouts.

---

## Sources

### Room Sizes - Residential
- [Standard Room Sizes - sqft.expert](https://sqft.expert/blogs/standard-sizes-of-rooms-architects)
- [Average Room Sizes - DesignFiles](https://blog.designfiles.co/average-room-size/)
- [Standard Room Sizes - Maramani](https://www.maramani.com/blogs/home-design-ideas/standard-room-sizes)
- [Standard Room Sizes - CiviConcepts](https://civiconcepts.com/blog/standard-room-size)
- [Average Room Size Guide - Leonayd](https://leonayd.com/average-room-size-guide-standard-dimensions/)
- [Standard Room Sizes - Homenish](https://www.homenish.com/standard-room-sizes/)
- [Standard Garage Size Guide - Alan's Factory Outlet](https://alansfactoryoutlet.com/blog/standard-garage-size/)
- [Entryway Dimensions - Angi](https://www.angi.com/articles/entryway-dimensions.htm)
- [Foyer Dimensions - Houzz](https://www.houzz.com/magazine/key-entryway-dimensions-for-homes-large-and-small-stsetivw-vs~25890082)
- [Foyer Size Guide - Plan7Architect](https://plan7architect.com/how-big-should-a-hallway-foyer-be-size-guide-ai2/)

### Room Sizes - Medical / Clinic
- [Square Footage by Room Type - Simour Design](https://simourdesign.com/square-footage-for-each-type-of-room/)
- [Sizing Exam Rooms - SpaceMed](https://blog.spacemed.com/sizing-exam-treatment-rooms/)
- [UNM Clinic Standards (PDF)](https://fm.unm.edu/standards--guidelines/documents/UNMH-Clinic-Standards_Revised04-12-10.pdf)
- [Medical Clinic Space Per Patient - BusinessDojo](https://dojobusiness.com/blogs/news/medical-clinic-room-requirements)
- [How Much Space Do You Need - Physicians Practice](https://www.physicianspractice.com/view/how-much-space-do-you-really-need)

### Room Sizes - Office
- [Office Space Calculator - AQUILA](https://aquilacommercial.com/learning-center/how-much-office-space-need-calculator-per-person/)
- [Average Office Size - YAROOMS](https://www.yarooms.com/blog/average-office-size-how-much-office-space-do-you-need)
- [Office Space Per Person - NBF](https://www.nationalbusinessfurniture.com/blog/how-much-office-space-per-person/)
- [Conference Room Size Guide - Archie](https://archieapp.co/blog/conference-room-size/)
- [Conference Room Sizes - Office Furniture Plus](https://www.officefurnitureplus.com/blog/conference-room-sizes-an-office-space-planning-guide/)

### Room Sizes - Police Station
- [IACP Police Facility Planning Guidelines (PDF)](https://www.theiacp.org/sites/default/files/2018-10/IACP_Police_Facility_Planning_Guidelines.pdf)
- [Police Space Needs Analysis - Town of Davidson (PDF)](https://www.townofdavidson.org/DocumentCenter/View/8941/Police-Space-Needs-Analysis)
- [Police Station Evidence Room Design - Fentress](https://blog.fentress.com/blog/best-design-practices-for-police-station-evidence-rooms)

### Room Sizes - Apartments
- [Studio Apartment Size Guide - Plan7Architect](https://plan7architect.com/how-big-should-a-studio-apartment-be-size-guide-ai2/)
- [Average Apartment Size - RentCafe](https://www.rentcafe.com/blog/rental-market/market-snapshots/national-average-apartment-size/)
- [Apartment Size Guide - Bigos](https://blog.tbigos.com/what-size-apartment-do-you-need/)
- [Large 2BR Apartment - House Solution Egypt](https://housesolutionegypt.com/blog/what-is-considered-a-large-2-bedroom-apartment)

### Room Sizes - Commercial / Retail / Restaurant
- [Restaurant Square Footage - Toast](https://pos.toasttab.com/blog/on-the-line/average-restaurant-square-footage)
- [Commercial Kitchen Size - Avanti](https://www.avanticorporate.com/blog/commercial-kitchen-sizes/)
- [Commercial Kitchen Size - Sam Tell](https://www.samtell.com/blog/average-commercial-kitchen-size)
- [Restaurant Floor Plans - TouchBistro](https://www.touchbistro.com/blog/15-restaurant-floor-plan-examples-and-tips/)

### Room Sizes - Warehouse
- [Warehouse Space Planning - WH1](https://www.wh1.com/warehouse-square-footage-tips/)
- [Loading Dock Design - MBC Management](https://mbcmusa.com/warehouse-loading-dock-design/)
- [Warehouse Design Guidelines - Logistics Bureau](https://www.logisticsbureau.com/warehouse-design-rules-of-thumb/)

### Room Sizes - Church
- [Church Space Dimensions - LifeWay](https://www.lifeway.com/en/articles/church-architecture-rules-thumb-space-dimensions)
- [Church Floor Plans - Wick Buildings](https://www.wickbuildings.com/blog/3-types-of-church-floor-plans-and-designs-small-mid-sized-and-big/)
- [Church Design Plans - General Steel](https://gensteel.com/building-faqs/preconstruction/church-design-plans/)

### Room Sizes - School
- [Average Classroom Size - SeatingChartMaker](https://seatingchartmaker.app/articles/average-classroom-size-square-feet/)
- [School Space Requirements - BusinessDojo](https://dojobusiness.com/blogs/news/private-school-space-requirements)
- [Classroom Design Standards - UConn (PDF)](https://updc.uconn.edu/wp-content/uploads/sites/1525/2020/09/Appendix-VI-Classroom-Design-Standards-August-2020.pdf)

### Building Codes
- [IBC 2024 Section 1208.2 - Minimum Room Sizes](https://codes.iccsafe.org/s/IBC2024P1/chapter-12-interior-environment/IBC2024P1-Ch12-Sec1208.2)
- [IBC Corridor Width Requirements - US Made Supply](https://usmadesupply.com/resources/building-codes-standards/emergency-life-safety/ibc-corridor-width)
- [Exit Corridor Width - Serbin Studio](https://serbinstudio.com/exit-corridor-width-how-to-determine-building-codes/)
- [ADA Door Requirements - ADA.gov](https://www.ada.gov/law-and-regs/design-standards/2010-stds/)
- [ADA Entrances, Doors, Gates - Access Board](https://www.access-board.gov/ada/guides/chapter-4-entrances-doors-and-gates/)
- [Minimum Dimensions in the IRC - Fine Homebuilding](https://www.finehomebuilding.com/2024/01/10/minimum-dimensions-in-the-irc)
- [Minimum Room Sizes and Ceiling Heights - EVstudio](https://evstudio.com/minimum-room-sizes-and-minimum-ceiling-heights/)

### Reference Books
- *Neufert Architects' Data* (Ernst Neufert, 5th Ed.) -- The gold standard reference for architectural programming and room dimensions. Not freely available online but referenced by most of the above sources.
