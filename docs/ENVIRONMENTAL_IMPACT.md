## 1. Context & Objective

Digital technology accounts for **4% of global greenhouse gas emissions**.  
As engineers, we must ensure StemHub is designed responsibly.

We analyzed our architecture through the three pillars of Eco-Conception:

- **Hosting**
- **Data Transfer**
- **Compute**

---

## 2. Eco-Conception Analysis

### A. Hosting: Low-Carbon Infrastructure

**Challenge:**  
Cloud servers and data centers are energy-intensive.

**StemHub Strategy:**  
We strategically select **AWS Regions with low carbon intensity** (e.g., `eu-west-3` Paris) to host our **S3 Buckets** and **PostgreSQL** database.

**Impact:**  
By leveraging grids powered by nuclear or hydroelectric energy, we significantly reduce the carbon footprint of our storage compared to coal-heavy regions.

---

### B. Data Transfer: The "Low-Hops" Architecture

**Challenge:**  
Transferring heavy audio files (`.wav`, stems) consumes massive network energy.

**StemHub Strategy:**  
We implemented **Signed URLs for Direct S3 Uploads** (bypassing the Python backend).

**Impact:**  
Files travel directly from:

Client → Storage (**1 Hop**)  
instead of  
Client → Server → Storage (**2 Hops**)

This halves the network hops, reducing energy consumption by approximately **50% per upload**.

---

### C. Compute: Efficient Algorithms & Client-Side Rendering

**Challenge:**  
Server-side processing of audio visuals is CPU-heavy.

**StemHub Strategy:**

- **Frontend:**  
  We use React and Wavesurfer.js to render waveforms directly on the user's device, relieving our servers of graphical processing.

- **Backend:**  
  We use FastAPI with optimized libraries (`struct`, `PyFLP`) to parse binary files efficiently, minimizing CPU cycles required per request.

---

## 3. Measuring Impact

To validate our strategy, we utilize the following tools:

- **EcoIndex:**  
  To audit our React frontend performance and aim for a score of **B or higher (70/100)**.

- **AWS Customer Carbon Footprint Tool:**  
  To estimate the CO2eq emissions of our AWS infrastructure.

---

## 4. Conclusion: Optimization Choices (Target)

| Category | Optimization Choice | Environmental Impact |
|----------|--------------------|----------------------|
| Hosting | AWS `eu-west-3` (Paris) | Drastic reduction of CO2eq emissions via nuclear power mix |
| Network | S3 Signed URLs | ~50% network energy reduction by bypassing the Python server |
| Compute | Client-Side Rendering | Offloads energy cost to client devices; reduces server heat/AC needs |
| Parsing | Binary Structs (PyFLP) | Minimizes CPU cycles/electricity required for file analysis |
| Storage | Lifecycle Policies | Automated cleanup of temporary files to reduce storage waste |
