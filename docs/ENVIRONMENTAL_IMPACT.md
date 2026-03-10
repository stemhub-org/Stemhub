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
We strategically select **Google Cloud Regions with low carbon intensity** (e.g., `europe-west9` Paris - Carbon Free Energy) to host our **GCS Buckets** and **PostgreSQL** database.

**Impact:**  
By leveraging Google's commitment to 24/7 carbon-free energy, we significantly reduce the carbon footprint of our storage and compute compared to traditional data centers.

---

### B. Data Transfer: The "Low-Hops" Architecture

**Challenge:**  
Transferring heavy audio files (`.wav`, stems) consumes massive network energy.

**StemHub Strategy:**  
We use **Streaming Processing** in our Python backend. Files are streamed directly from the client through the server to **Google Cloud Storage (GCS)** without saving temporary copies on disk.

**Impact:**  
This avoids extra "Disk Write/Read" cycles on the server, saving I/O-related energy. While it adds a server hop for validation (Security/Consistency), the lack of intermediate disk persistence keeps the energy footprint per GB optimized.

---

### C. Compute: Efficient Algorithms & Client-Side Rendering

**Challenge:**  
Server-side processing of audio visuals is CPU-heavy.

**StemHub Strategy:**

- **Frontend:**  
  We use React and Wavesurfer.js to render waveforms directly on the user's device, relieving our servers of graphical processing.

- **Backend:**  
  We use FastAPI with optimized libraries (`struct`, `PyFLP_enhanced`) and **Alembic** managed migrations to ensure efficient execution and minimal database overhead.

---

## 3. Measuring Impact

To validate our strategy, we utilize the following tools:

- **EcoIndex:**  
  To audit our React frontend performance and aim for a score of **B or higher (70/100)**.

- **Google Cloud Carbon Footprint:**  
  To monitor and minimize the CO2eq emissions of our GCP infrastructure.

---

## 4. Conclusion: Optimization Choices (Target)

| Category | Optimization Choice | Environmental Impact |
|----------|--------------------|----------------------|
| Hosting | GCP `europe-west9` (Paris) | 100% carbon-neutral energy (matched) |
| Network | Streaming Uploads | Zero intermediate disk I/O, optimized energy per GB |
| Compute | Client-Side Rendering | Offloads energy cost to client devices; reduces server heat/AC needs |
| Parsing | Binary Structs (PyFLP_enhanced) | Minimizes CPU cycles/electricity required for file analysis |
| Storage | GCS Lifecycle Policies | Automated cleanup of temporary files to reduce storage waste |
