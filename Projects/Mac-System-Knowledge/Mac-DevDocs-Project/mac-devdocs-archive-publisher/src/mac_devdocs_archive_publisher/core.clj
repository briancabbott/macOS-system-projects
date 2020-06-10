(ns mac-devdocs-archive-publisher.core
  (:require [clojure.pprint :as pp]
            [clojure.data.json :as json]
            [org.httpkit.client :as http]
            [cheshire.core :refer :all])

  (:gen-class))

(defn keyfn [key]
  ; (println (str "keyFN: "))
  ; (pp/pprint key)
  (keyword key)
  )

(defn valfn [key value]
  ; (println (str "key: "))
  ; (pp/pprint key)
  ; (println (str "value: "))
  ; (pp/pprint value)
  value)

(defn get-library-def []
  (let [liburl "https://developer.apple.com/library/archive/navigation/library.json"
        options {:timeout 10000             ; ms
                 :user-agent "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_5) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/83.0.4103.97 Safari/537.36"}]
    (println "starting call")
    (http/get liburl options (fn [{:keys [status headers body error]}]
                               (println "into callback handler")
                               (if error
                                 (println "Failed, exception is " error)
                                 (do
                                   
                                   (spit "received-library.json" body)
                                   (let [rdr (parse-string body)] ; (json/read-str body :key-fn keyfn :value-fn valfn)]
                                     ; (println "object parsed, press something to see it:")
                                     ; (read-line)
                                     (pp/pprint rdr))))))))

(defn -main
  "I don't do a whole lot ... yet."
  [& args]
  (get-library-def)
  (read-line))
